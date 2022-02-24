// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

/* region_allocator.h -- region allocator header */

#ifndef LIBPMEMSTREAM_REGION_ALLOCATOR_H
#define LIBPMEMSTREAM_REGION_ALLOCATOR_H

#include "libpmemstream_internal.h"

#include <stdint.h>
#include <string.h>

extern const uint64_t EMPTY_OBJ;
struct singly_linked_list {
	uint64_t head;
	uint64_t tail;
};
struct allocator_header {
	uint64_t magic;
	struct singly_linked_list free_list;
	struct singly_linked_list alloc_list;
	uint64_t padding[3];
	uint64_t data[];
};

#ifdef __cplusplus
extern "C" {
#endif

/* API */
// uint64_t allocate_region(struct allocator_header *alloc_header, uint64_t region_size);
// void free_region(struct allocator_header *alloc_header, uint64_t region_offset);
uint64_t get_iterator_for_type(struct allocator_header *alloc_header, enum span_type type);
uint64_t next(struct allocator_header *alloc_header, uint64_t it);

void region_allocator_new(struct allocator_header **alloc_header, uint64_t *ptr, size_t size);
uint64_t region_allocator_allocate(struct allocator_header *alloc_header, uint64_t size, uint64_t alignment);
uint64_t *pointer_to_offset(uint64_t *base, uint64_t offset);
uint64_t split(struct allocator_header *alloc_header, uint64_t it, size_t size);

/* Singly-linked list API */
uint64_t remove_head(struct allocator_header *alloc_header, struct singly_linked_list *list);
void push_front(struct allocator_header *alloc_header, struct singly_linked_list *list, uint64_t offset);
void push_back(struct allocator_header *alloc_header, struct singly_linked_list *list, uint64_t offset);
uint64_t set_next(struct allocator_header *alloc_header, uint64_t it, uint64_t next_offset);
uint64_t next(struct allocator_header *alloc_header, uint64_t it);

#ifdef __cplusplus
} /* end extern "C" */
#endif
#endif /* LIBPMEMSTREAM_REGION_ALLOCATOR_H */
