// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include "span.h"

#include <assert.h>

#include "libpmemstream_internal.h"

span_bytes *span_offset_to_span_ptr(struct pmemstream *stream, uint64_t offset)
{
	assert(offset % sizeof(span_bytes) == 0);

	return (span_bytes *)pmemstream_offset_to_ptr(stream, offset);
}

void span_create_empty(struct pmemstream *stream, uint64_t offset, size_t data_size)
{
	span_bytes *span = span_offset_to_span_ptr(stream, offset);
	assert((data_size & SPAN_TYPE_MASK) == 0);
	span[0] = data_size | SPAN_EMPTY;

	void *dest = ((uint8_t *)span) + SPAN_EMPTY_METADATA_SIZE;
	stream->memset(dest, 0, data_size, PMEM2_F_MEM_NONTEMPORAL | PMEM2_F_MEM_NODRAIN);
	stream->persist(span, SPAN_EMPTY_METADATA_SIZE);
}

/* XXX: add descr
 *
 * flags may be used to adjust behavior of persisting the data; use 0 for default persist.
 */
void span_create_entry(struct pmemstream *stream, uint64_t offset, size_t data_size, size_t popcount, int flags)
{
	span_bytes *span = span_offset_to_span_ptr(stream, offset);
	assert((data_size & SPAN_TYPE_MASK) == 0);

	// XXX - use variadic mempcy to store data and metadata at once
	span[0] = data_size | SPAN_ENTRY;
	span[1] = popcount;

	size_t persist_size = data_size + SPAN_ENTRY_METADATA_SIZE;
	if (flags & PMEMSTREAM_PUBLISH_NOFLUSH_DATA) {
		persist_size = SPAN_ENTRY_METADATA_SIZE;
	} else if (flags & PMEMSTREAM_PUBLISH_NOFLUSH) {
		persist_size = 0;
	}

	if (persist_size != 0) {
		stream->persist(span, persist_size);
	}
}

void span_create_region(struct pmemstream *stream, uint64_t offset, size_t size)
{
	span_bytes *span = span_offset_to_span_ptr(stream, offset);
	assert((size & SPAN_TYPE_MASK) == 0);
	span[0] = size | SPAN_REGION;

	stream->persist(span, SPAN_REGION_METADATA_SIZE);
}

uint64_t span_get_size(span_bytes *span)
{
	return span[0] & SPAN_EXTRA_MASK;
}

enum span_type span_get_type(span_bytes *span)
{
	return span[0] & SPAN_TYPE_MASK;
}

struct span_runtime span_get_empty_runtime(struct pmemstream *stream, uint64_t offset)
{
	span_bytes *span = span_offset_to_span_ptr(stream, offset);
	struct span_runtime srt;

	assert(span_get_type(span) == SPAN_EMPTY);

	srt.type = SPAN_EMPTY;
	srt.empty.size = span_get_size(span);
	srt.data_offset = offset + SPAN_EMPTY_METADATA_SIZE;
	srt.total_size = ALIGN_UP(srt.empty.size + SPAN_EMPTY_METADATA_SIZE, sizeof(span_bytes));

	return srt;
}

struct span_runtime span_get_entry_runtime(struct pmemstream *stream, uint64_t offset)
{
	span_bytes *span = span_offset_to_span_ptr(stream, offset);
	struct span_runtime srt;

	assert(span_get_type(span) == SPAN_ENTRY);

	srt.type = SPAN_ENTRY;
	srt.entry.size = span_get_size(span);
	srt.entry.popcount = span[1];
	srt.data_offset = offset + SPAN_ENTRY_METADATA_SIZE;
	srt.total_size = ALIGN_UP(srt.entry.size + SPAN_ENTRY_METADATA_SIZE, sizeof(span_bytes));

	return srt;
}

struct span_runtime span_get_region_runtime(struct pmemstream *stream, uint64_t offset)
{
	span_bytes *span = span_offset_to_span_ptr(stream, offset);
	struct span_runtime srt;

	assert(span_get_type(span) == SPAN_REGION);

	srt.type = SPAN_REGION;
	srt.region.size = span_get_size(span);
	srt.data_offset = offset + SPAN_REGION_METADATA_SIZE;
	srt.total_size = ALIGN_UP(srt.region.size + SPAN_REGION_METADATA_SIZE, sizeof(span_bytes));

	return srt;
}

struct span_runtime span_get_runtime(struct pmemstream *stream, uint64_t offset)
{
	span_bytes *span = span_offset_to_span_ptr(stream, offset);
	struct span_runtime srt;

	switch (span_get_type(span)) {
		case SPAN_EMPTY:
			srt = span_get_empty_runtime(stream, offset);
			break;
		case SPAN_ENTRY:
			srt = span_get_entry_runtime(stream, offset);
			break;
		case SPAN_REGION:
			srt = span_get_region_runtime(stream, offset);
			break;
		default:
			abort();
	}

	return srt;
}
