// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

/* Internal Header */

#ifndef LIBPMEMSTREAM_INTERNAL_H
#define LIBPMEMSTREAM_INTERNAL_H

#include "iterator.h"
#include "libpmemstream.h"
#include "region.h"
#include "span.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PMEMSTREAM_SIGNATURE ("PMEMSTREAM")
#define PMEMSTREAM_SIGNATURE_SIZE (64)

struct pmemstream_data {
	struct pmemstream_header {
		char signature[PMEMSTREAM_SIGNATURE_SIZE];
		uint64_t stream_size;
		uint64_t block_size;
	} header;
	span_bytes spans[];
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

	struct region_contexts_map *region_contexts_map;
};

static inline uint8_t *pmemstream_offset_to_ptr(struct pmemstream *stream, size_t offset)
{
	return (uint8_t *)stream->data->spans + offset;
}

#ifdef __cplusplus
} /* end extern "C" */
#endif
#endif /* LIBPMEMSTREAM_INTERNAL_H */
