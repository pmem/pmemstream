// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

/* Internal Header */

#ifndef LIBPMEMSTREAM_INTERNAL_H
#define LIBPMEMSTREAM_INTERNAL_H

#include <assert.h>
#include <semaphore.h>

#include <libminiasync.h>

#include "iterator.h"
#include "libpmemstream.h"
#include "mpmc_queue.h"
#include "pmemstream_runtime.h"
#include "region.h"
#include "region_allocator/allocator_base.h"
#include "span.h"
#include "thread_id.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PMEMSTREAM_SIGNATURE ("PMEMSTREAM")
#define PMEMSTREAM_SIGNATURE_SIZE (64)

/* In some cases we relay on incrementing timestamp by 1.
 * Because of that we require FIRST timestamp to be exactly "1 away" from INVALID. */
#define PMEMSTREAM_INVALID_TIMESTAMP (0ULL)
#define PMEMSTREAM_FIRST_TIMESTAMP (PMEMSTREAM_INVALID_TIMESTAMP + 1ULL)
static_assert(PMEMSTREAM_INVALID_TIMESTAMP + 1 == PMEMSTREAM_FIRST_TIMESTAMP, "wrong timestamp's macros values");

/* XXX: make this a parameter */
#define PMEMSTREAM_MAX_CONCURRENCY 1024ULL

struct pmemstream_header {
	char signature[PMEMSTREAM_SIGNATURE_SIZE];
	uint64_t stream_size;
	uint64_t block_size;

	/* XXX: investigate if it makes sense to store 'shadow' value in DRAM (for reads) */
	/* XXX: we can 'distribute' persisted_timestamp and store multiple variables (one per thread) to speed up writes
	 */
	/* All entries with timestamps less than or equal to 'persisted_timestamp' can be treated as persisted. */
	uint64_t persisted_timestamp;

	struct allocator_header region_allocator_header;
};

/* Description of an async operation. */
struct async_operation {
	/* Data memcpy future */
	struct vdm_operation_future future;

	/* Description of append operation. */
	struct pmemstream_region region;
	struct pmemstream_entry entry;
	size_t size;

	struct pmemstream_region_runtime *region_runtime;
};

struct pmemstream {
	/* Points to pmem-resided header. */
	struct pmemstream_header *header;

	/* Describes data location and memory operations. */
	struct pmemstream_runtime data;

	size_t stream_size;
	size_t usable_size;
	size_t block_size;

	struct region_runtimes_map *region_runtimes_map;

	/* All entries with timestamps less than or equal to 'committed_timestamp' can be treated as committed. */
	alignas(CACHELINE_SIZE) uint64_t committed_timestamp;

	/* This timestamp is used to generate timestamps for append. It is always increased monotonically. */
	alignas(CACHELINE_SIZE) uint64_t next_timestamp;

	/* Protects slots in async_ops array. */
	pthread_mutex_t *async_ops_locks;

	/* Stores in-progress operations, indexed by timestamp mod array size. */
	struct async_operation *async_ops;

	/* Protects increasing committed timestamp. */
	pthread_mutex_t commit_lock;

	/* Used to perform synchronous memcpy. */
	struct data_mover_sync *data_mover_sync;

	/* Protects against exceeding PMEMSTREAM_MAX_CONCURRENCY. */
	sem_t async_ops_semaphore;
};

static inline int pmemstream_validate_stream_and_offset(struct pmemstream *stream, uint64_t offset)
{
	if (!stream) {
		return -1;
	}
	if (stream->header->stream_size <= offset) {
		return -1;
	}
	return 0;
}

/* Convert offset to pointer to span. offset must be 8-bytes aligned. */
static inline const struct span_base *span_offset_to_span_ptr(const struct pmemstream_runtime *data, uint64_t offset)
{
	assert(offset % sizeof(struct span_base) == 0);
	return (const struct span_base *)pmemstream_offset_to_ptr(data, offset);
}

#ifdef __cplusplus
} /* end extern "C" */
#endif
#endif /* LIBPMEMSTREAM_INTERNAL_H */
