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

	/* Memory after this offset is not yet tracked by any list. */
	uint64_t free_offset;

	uint64_t size;

	/* If != SLIST_INVALID_OFFSET it means there was a crash and it contains an offset of element which was being
	 * freed. */
	uint64_t recovery_free_offset;
};

struct allocator_entry_metadata {
	uint64_t next_allocated;
	uint64_t next_free;
};

static inline void allocator_initialize(const struct pmemstream_runtime *runtime, struct allocator_header *header,
					size_t size)
{
	header->free_offset = 0;
	header->size = size;
	header->recovery_free_offset = SLIST_INVALID_OFFSET;

	runtime->flush(&header->free_offset, sizeof(header->free_offset));
	runtime->flush(&header->size, sizeof(header->size));
	runtime->flush(&header->recovery_free_offset, sizeof(header->recovery_free_offset));
	runtime->drain();

	SLIST_INIT(runtime, &header->free_list);
	SLIST_INIT(runtime, &header->allocated_list);
}

#ifdef __cplusplus
} /* end extern "C" */
#endif
#endif /* LIBPMEMSTREAM_ALLOCATOR_BASE_H */
