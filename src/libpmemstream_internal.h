// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

/* Internal Header */

#ifndef LIBPMEMSTREAM_INTERNAL_H
#define LIBPMEMSTREAM_INTERNAL_H

#include "common/util.h"
#include "libpmemstream.h"
#include <libpmem2.h>

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
	uint8_t *data;
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
};

struct pmemstream_entry_iterator {
	struct pmemstream *stream;
	struct pmemstream_region region;
	size_t offset;
};

struct pmemstream_region_iterator {
	struct pmemstream *stream;
	struct pmemstream_region region;
};

int pmemstream_memcpy_impl(pmem2_memcpy_fn pmem2_memcpy, void *destination, const size_t argc, ...);

#define pmemstream_memcpy(pmem2_memcpy, dest, ...)                                                                     \
	pmemstream_memcpy_impl(pmem2_memcpy, dest, COUNT(__VA_ARGS__), __VA_ARGS__)

#ifdef __cplusplus
} /* end extern "C" */
#endif
#endif /* LIBPMEMSTREAM_INTERNAL_H */
