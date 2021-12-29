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

enum span_type { SPAN_EMPTY = 00ULL << 62, SPAN_REGION = 11ULL << 62, SPAN_ENTRY = 10ULL << 62 };

#define SPAN_TYPE_MASK (11ULL << 62)
#define SPAN_EXTRA_MASK (~SPAN_TYPE_MASK)

struct span_runtime {
	enum span_type type;
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

#define SPAN_EMPTY_METADATA_SIZE (MEMBER_SIZE(span_runtime, empty))
#define SPAN_REGION_METADATA_SIZE (MEMBER_SIZE(span_runtime, region))
#define SPAN_ENTRY_METADATA_SIZE (MEMBER_SIZE(span_runtime, entry))

typedef uint64_t span_bytes;

span_bytes *span_offset_to_span_ptr(struct pmemstream *stream, size_t offset);
void span_create_empty(struct pmemstream *stream, uint64_t offset, size_t data_size);
void span_create_entry(struct pmemstream *stream, uint64_t offset, const void *data, size_t data_size, size_t popcount);
void span_create_region(struct pmemstream *stream, uint64_t offset, size_t size);
uint64_t span_get_size(span_bytes *span);
enum span_type spend_get_type(span_bytes *span);
struct span_runtime span_get_empty_runtime(struct pmemstream *stream, uint64_t offset);
struct span_runtime span_get_entry_runtime(struct pmemstream *stream, uint64_t offset);
struct span_runtime span_get_region_runtime(struct pmemstream *stream, uint64_t offset);
struct span_runtime span_get_runtime(struct pmemstream *stream, uint64_t offset);

#ifdef __cplusplus
} /* end extern "C" */
#endif
#endif /* LIBPMEMSTREAM_SPAN_H */
