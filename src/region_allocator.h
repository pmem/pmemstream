// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

/* region_allocator.h -- region allocator header */

#ifndef LIBPMEMSTREAM_REGION_ALLOCATOR_H
#define LIBPMEMSTREAM_REGION_ALLOCATOR_H

#include "libpmemstream_internal.h"

#include <stdint.h>
#include <string.h>

extern const uint64_t EMPTY_OBJ;

struct allocator_header {
	uint64_t magic;
	uint64_t first_free;	  // first empty span = 0
	uint64_t first_allocated; // first allocated region offset
	uint64_t padding[5];
	uint64_t data[];
};

#ifdef __cplusplus
extern "C" {
#endif

/* API */
uint64_t allocate_region(uint64_t region_size);
void free_region(uint64_t region_offset);
uint64_t get_region_iterator();
uint64_t get_empty_iterator();
uint64_t next(uint64_t it);

void initialize_memory(uint64_t *ptr, size_t size);
uint64_t *pointer_to_offset(uint64_t *base, uint64_t offset);
int is_free_empty(uint64_t *ptr);
uint64_t split(uint64_t *ptr, size_t size);

#ifdef __cplusplus
} /* end extern "C" */
#endif
#endif
