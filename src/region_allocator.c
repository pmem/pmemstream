// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

/* region_allocator.h -- region allocator header */

#include "region_allocator.h"

#include "libpmemstream_internal.h"
#include "span.h"

#include <stdint.h>
#include <string.h>

const uint64_t EMPTY_OBJ = UINT64_MAX;

struct allocator_header *alloc_header;

void initialize_memory(uint64_t *ptr, size_t size)
{
	const uint64_t header_id = 26985;
	alloc_header = (struct allocator_header *)ptr;
	if (alloc_header->magic == header_id)
		return;

	alloc_header->first_free = 0;
	alloc_header->first_allocated = EMPTY_OBJ;
	alloc_header->magic = header_id;

	struct span_empty se;
	se.span_base = span_base_create(size - sizeof(struct span_region), SPAN_EMPTY);
	memcpy(alloc_header->data, &se, sizeof(se));
}

uint64_t *pointer_to_offset(uint64_t *base, uint64_t offset)
{
	return (uint64_t *)((uint8_t *)base + offset);
}

int is_free_empty(uint64_t *ptr)
{
	if (span_get_type((struct span_base *)&ptr[0]) == SPAN_EMPTY) { // retrieve first element from list
		return 1;
	}
	return 0;
}

uint64_t split(uint64_t *ptr, size_t size)
{
	if (alloc_header->first_free == EMPTY_OBJ) {
		return EMPTY_OBJ;
	}
	uint64_t free_size = span_get_total_size(
		(struct span_base *)pointer_to_offset(alloc_header->data, alloc_header->first_free));
	if (size > free_size + sizeof(struct span_empty)) {
		return EMPTY_OBJ;
	}
	struct span_empty se[2] = {0};
	size_t target_size[2] = {size, free_size - size}; // size plus metadata size (for empty or region?)
	uint64_t target_offset[2] = {alloc_header->first_free, alloc_header->first_free + size};
	se[0].span_base = span_base_create(target_size[0], SPAN_EMPTY);
	se[1].span_base = span_base_create(target_size[1], SPAN_EMPTY);
	se[0].next = target_offset[1];
	memcpy(pointer_to_offset(alloc_header->data, target_offset[0]), &se[0], sizeof(struct span_empty));
	memcpy(pointer_to_offset(alloc_header->data, target_offset[1]), &se[1], sizeof(struct span_empty));
	return 0;
}
