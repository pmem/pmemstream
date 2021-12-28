// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

/* Internal Header */

#ifndef LIBPMEMSTREAM_SPAN_H
#define LIBPMEMSTREAM_SPAN_H

#include "common/util.h"
#include "libpmemstream.h"

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

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

pmemstream_span_bytes *pmemstream_offset_to_span_ptr(struct pmemstream *stream, size_t offset);
void pmemstream_span_create_empty(struct pmemstream *stream, uint64_t offset, size_t data_size);
void pmemstream_span_create_entry(struct pmemstream *stream, uint64_t offset, const void *data, size_t data_size,
				  size_t popcount);
void pmemstream_span_create_region(struct pmemstream *stream, uint64_t offset, size_t size);
uint64_t pmemstream_get_span_size(pmemstream_span_bytes *span);
enum pmemstream_span_type pmemstream_get_span_type(pmemstream_span_bytes *span);
struct pmemstream_span_runtime pmemstream_span_get_empty_runtime(struct pmemstream *stream, uint64_t offset);
struct pmemstream_span_runtime pmemstream_span_get_entry_runtime(struct pmemstream *stream, uint64_t offset);
struct pmemstream_span_runtime pmemstream_span_get_region_runtime(struct pmemstream *stream, uint64_t offset);
struct pmemstream_span_runtime pmemstream_span_get_runtime(struct pmemstream *stream, uint64_t offset);

#ifdef __cplusplus
} /* end extern "C" */
#endif
#endif /* LIBPMEMSTREAM_SPAN_H */
