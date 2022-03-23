// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#ifndef LIBPMEMSTREAM_ALLOCATOR_BASE_H
#define LIBPMEMSTREAM_ALLOCATOR_BASE_H

#include "pmemstream_runtime.h"
#include "singly_linked_list.h"

#ifdef __cplusplus
extern "C" {
#endif

struct allocator_header {
	struct singly_linked_list free_list;
	struct singly_linked_list allocated_list;

	uint64_t free_offset;
	uint64_t total_size;

	uint64_t offset_to_free;
};

struct allocator_metadata {
	uint64_t next_allocated;
	uint64_t next_free;
};

static inline void allocator_initialize(const struct pmemstream_runtime *runtime, struct allocator_header *header,
					size_t size)
{
	header->free_offset = 0;
	header->total_size = size;
	header->offset_to_free = SLIST_INVALID_OFFSET;

	runtime->flush(&header->free_offset, sizeof(header->free_offset));
	runtime->flush(&header->total_size, sizeof(header->total_size));
	runtime->flush(&header->offset_to_free, sizeof(header->offset_to_free));
	runtime->drain();

	SLIST_INIT(runtime, &header->free_list);
	SLIST_INIT(runtime, &header->allocated_list);
}

#ifdef __cplusplus
} /* end extern "C" */
#endif
#endif
