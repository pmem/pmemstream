// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

/* Internal Header */

#ifndef LIBPMEMSTREAM_INTERNAL_H
#define LIBPMEMSTREAM_INTERNAL_H

#include "critnib/critnib.h"
#include "libpmemstream.h"

#include <libpmem2.h>

#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MEMBER_SIZE(type, member) sizeof(((struct type *)NULL)->member)

enum pmemstream_span_type {
	PMEMSTREAM_SPAN_EMPTY = 00ULL << 62,
	PMEMSTREAM_SPAN_REGION = 11ULL << 62,
	PMEMSTREAM_SPAN_ENTRY = 10ULL << 62
};

#define PMEMSTREAM_SPAN_TYPE_MASK (11ULL << 62)
#define PMEMSTREAM_SPAN_EXTRA_MASK (~PMEMSTREAM_SPAN_TYPE_MASK)

struct pmemstream_span_runtime {
	enum pmemstream_span_type type;
	size_t total_size;
	uint64_t data_offset;
	union {
		struct {
			uint64_t size;
		} empty;
		struct {
			uint64_t size;
		} region;
		struct {
			uint64_t size;
			uint64_t popcount;
		} entry;
	};
};

#define SPAN_EMPTY_METADATA_SIZE (MEMBER_SIZE(pmemstream_span_runtime, empty))
#define SPAN_REGION_METADATA_SIZE (MEMBER_SIZE(pmemstream_span_runtime, region))
#define SPAN_ENTRY_METADATA_SIZE (MEMBER_SIZE(pmemstream_span_runtime, entry))

typedef uint64_t pmemstream_span_bytes;

#define PMEMSTREAM_SIGNATURE ("PMEMSTREAM")
#define PMEMSTREAM_SIGNATURE_SIZE (64)

struct pmemstream_data {
	struct pmemstream_header {
		char signature[PMEMSTREAM_SIGNATURE_SIZE];
		uint64_t stream_size;
		uint64_t block_size;
	} header;
	uint64_t spans[];
};

struct pmemstream {
	struct pmemstream_data *data;
	size_t stream_size;
	size_t usable_size;
	size_t block_size;

	pmem2_memcpy_fn memcpy;
	pmem2_memset_fn memset;
	pmem2_flush_fn flush;
	pmem2_drain_fn drain;
	pmem2_persist_fn persist;

	critnib *region_contexts_container;
	pthread_mutex_t region_contexts_container_lock;
};

struct pmemstream_entry_iterator {
	struct pmemstream *stream;
	struct pmemstream_region region;
	struct pmemstream_region_context *region_context;
	size_t offset;
};

struct pmemstream_region_iterator {
	struct pmemstream *stream;
	struct pmemstream_region region;
};

#define PMEMSTREAM_OFFSET_UNITINIALIZED 0ULL

/*
 * It contains all runtime data specific to a region.
 * It is always managed by the pmemstream (user can only obtain a non-owning pointer) and can be created
 * in few different ways:
 * - By explicitly calling pmemstream_get_region_context() for the first time
 * - By calling pmemstream_append (only if region_context does not exist yet)
 * - By advancing an entry iterator past last entry in a region (only if region_context does not exist yet)
 */
struct pmemstream_region_context {
	/*
	 * Offset at which new entries will be appended. If set to PMEMSTREAM_OFFSET_UNITINIALIZED it means
	 * that region was not yet recovered. */
	uint64_t append_offset;
};

#ifdef __cplusplus
} /* end extern "C" */
#endif
#endif /* LIBPMEMSTREAM_INTERNAL_H */
