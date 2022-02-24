// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

/* region_allocator.h -- region allocator header */

#include "region_allocator.h"

#include "common/util.h"
#include "libpmemstream_internal.h"
#include "span.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

const uint64_t EMPTY_OBJ = UINT64_MAX;

uint64_t *pointer_to_offset(uint64_t *base, uint64_t offset)
{
	return (uint64_t *)((uint8_t *)base + offset);
}

void convert_type(uint64_t *span_header, enum span_type type)
{
	*span_header &= SPAN_EXTRA_MASK;
	*span_header |= type;
}

uint64_t get_iterator_for_type(struct allocator_header *alloc_header, enum span_type type)
{
	if (type == SPAN_REGION) {
		return alloc_header->alloc_list.head;
	} else if (type == SPAN_EMPTY) {
		return alloc_header->free_list.head;
	}
	return UINT64_MAX;
}

uint64_t next(struct allocator_header *alloc_header, uint64_t it)
{
	if (it != UINT64_MAX) {
		struct span_base *sb = (struct span_base *)pointer_to_offset(alloc_header->data, it);
		switch ((uint64_t)(sb->size_and_type & SPAN_TYPE_MASK)) {
			case SPAN_EMPTY:
				return ((struct span_empty *)sb)->next;
			case SPAN_REGION:
				return ((struct span_region *)sb)->next;
			default:
				break;
		}
	}
	return UINT64_MAX;
}

uint64_t set_next(struct allocator_header *alloc_header, uint64_t it, uint64_t next_offset)
{
	if (it == UINT64_MAX)
		return UINT64_MAX;
	struct span_base *sb = (struct span_base *)pointer_to_offset(alloc_header->data, it);
	switch ((uint64_t)(sb->size_and_type & SPAN_TYPE_MASK)) {
		case SPAN_EMPTY:
			((struct span_empty *)sb)->next = next_offset;
			break;
		case SPAN_REGION:
			((struct span_region *)sb)->next = next_offset;
			break;
		default:
			return UINT64_MAX;
	}
	return 0;
}

uint64_t remove_head(struct allocator_header *alloc_header, struct singly_linked_list *list)
{
	if (list->head == EMPTY_OBJ)
		return EMPTY_OBJ;
	uint64_t to_be_deleted = list->head;
	list->head = next(alloc_header, list->head);
	return to_be_deleted;
}

void remove_elem(struct allocator_header *alloc_header, struct singly_linked_list *list, uint64_t it)
{
	if (list->head == it) {
		remove_head(alloc_header, list);
	} else {
		uint64_t current_element = list->head;
		uint64_t prev = 0;
		while ((current_element = next(alloc_header, current_element)) != it) {
			prev = current_element;
		}
		set_next(alloc_header, prev, next(alloc_header, it));
	}
}

void push_front(struct allocator_header *alloc_header, struct singly_linked_list *list, uint64_t offset)
{
	if (list->head == list->tail) {
		list->tail = offset;
	}
	set_next(alloc_header, offset, list->head);
	list->head = offset;
}

void push_back(struct allocator_header *alloc_header, struct singly_linked_list *list, uint64_t offset)
{
	if (list->head == EMPTY_OBJ && list->tail == EMPTY_OBJ) {
		list->head = offset;
		list->tail = offset;
		set_next(alloc_header, list->tail, EMPTY_OBJ);
		return;
	}
	set_next(alloc_header, list->tail, offset);
	list->tail = offset;
}

void region_allocator_new(struct allocator_header **alloc_header, uint64_t *ptr, size_t size)
{
	const uint64_t header_id = 26985;
	*alloc_header = (struct allocator_header *)ptr;
	if ((*alloc_header)->magic == header_id)
		return;

	(*alloc_header)->free_list.head = 0;
	(*alloc_header)->free_list.tail = 0;
	(*alloc_header)->alloc_list.head = EMPTY_OBJ;
	(*alloc_header)->alloc_list.tail = EMPTY_OBJ;
	(*alloc_header)->magic = header_id;

	struct span_empty se = {.span_base = span_base_create(size - sizeof(struct span_region), SPAN_EMPTY),
				.next = EMPTY_OBJ};
	memcpy((*alloc_header)->data, &se, sizeof(se));
}

uint64_t split(struct allocator_header *alloc_header, uint64_t it, size_t size)
{
	if (it == EMPTY_OBJ) {
		return EMPTY_OBJ;
	}

	uint64_t free_size = span_get_total_size((struct span_base *)pointer_to_offset(alloc_header->data, it));
	if (size > free_size + sizeof(struct span_empty)) {
		return EMPTY_OBJ;
	}
	struct span_empty se[2] = {0};
	size_t target_size[2] = {size, free_size - size};
	uint64_t target_offset[2] = {it, it + size};
	se[0].span_base.size_and_type = target_size[0];
	se[1].span_base.size_and_type = target_size[1];
	se[0].next = target_offset[1];
	se[1].next = next(alloc_header, it);
	memcpy(pointer_to_offset(alloc_header->data, target_offset[0]), &se[0], sizeof(struct span_empty));
	memcpy(pointer_to_offset(alloc_header->data, target_offset[1]), &se[1], sizeof(struct span_empty));

	/* Tail update */
	struct span_base *it_sb = (struct span_base *)pointer_to_offset(alloc_header->data, it);
	uint64_t *tail = NULL;
	switch (span_get_type(it_sb)) {
		case SPAN_EMPTY:
			tail = &(alloc_header->free_list.tail);
			break;
		case SPAN_REGION:
			tail = &(alloc_header->alloc_list.tail);
			break;
		default:
			break;
	}
	if (*tail == it) {
		*tail = target_offset[1];
	}
	return target_offset[1];
}

uint64_t region_allocator_allocate(struct allocator_header *alloc_header, uint64_t size, uint64_t alignment)
{
	size = ALIGN_UP(size, alignment);
	uint64_t free_block_size = span_get_total_size(
		(struct span_base *)pointer_to_offset(alloc_header->data, alloc_header->free_list.head));
	if (size != free_block_size && size < free_block_size) {
		split(alloc_header, alloc_header->free_list.head, size);
	}
	uint64_t removed_elem = remove_head(alloc_header, &alloc_header->free_list);
	push_back(alloc_header, &alloc_header->alloc_list, removed_elem);
	set_next(alloc_header, removed_elem, EMPTY_OBJ);
	convert_type(pointer_to_offset(alloc_header->data, alloc_header->alloc_list.tail), SPAN_REGION);
	return alloc_header->alloc_list.tail;
}

void region_allocator_release(struct allocator_header *alloc_header, uint64_t offset)
{
	struct span_base *span_b = (struct span_base *)pointer_to_offset(alloc_header->data, offset);
#ifndef NDEBUG
	enum span_type span_t = span_get_type(span_b);
	assert(span_t == SPAN_EMPTY || span_t == SPAN_REGION);
#endif
	push_front(alloc_header, &alloc_header->free_list, offset);
	convert_type(&span_b->size_and_type, SPAN_EMPTY);
	remove_elem(alloc_header, &alloc_header->alloc_list, offset);
}
