// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

#include "span.h"
#include "libpmemstream_internal.h"

#include <assert.h>

uint8_t *pmemstream_offset_to_ptr(struct pmemstream *stream, size_t offset)
{
	return (uint8_t *)stream->data->spans + offset;
}

pmemstream_span_bytes *pmemstream_offset_to_span_ptr(struct pmemstream *stream, size_t offset)
{
	assert(offset % sizeof(pmemstream_span_bytes) == 0);

	return (pmemstream_span_bytes *)pmemstream_offset_to_ptr(stream, offset);
}

void pmemstream_span_create_empty(struct pmemstream *stream, uint64_t offset, size_t data_size)
{
	pmemstream_span_bytes *span = pmemstream_offset_to_span_ptr(stream, offset);
	assert((data_size & PMEMSTREAM_SPAN_TYPE_MASK) == 0);
	span[0] = data_size | PMEMSTREAM_SPAN_EMPTY;

	void *dest = ((uint8_t *)span) + SPAN_EMPTY_METADATA_SIZE;
	stream->memset(dest, 0, data_size, PMEM2_F_MEM_NONTEMPORAL | PMEM2_F_MEM_NODRAIN);
	stream->persist(span, SPAN_EMPTY_METADATA_SIZE);
}

void pmemstream_span_create_entry(struct pmemstream *stream, uint64_t offset, const void *data, size_t data_size,
				  size_t popcount)
{
	pmemstream_span_bytes *span = pmemstream_offset_to_span_ptr(stream, offset);
	assert((data_size & PMEMSTREAM_SPAN_TYPE_MASK) == 0);
	span[0] = data_size | PMEMSTREAM_SPAN_ENTRY;
	span[1] = popcount;

	// XXX - use variadic mempcy to store data and metadata at once
	void *dest = ((uint8_t *)span) + SPAN_ENTRY_METADATA_SIZE;
	stream->memcpy(dest, data, data_size, PMEM2_F_MEM_NONTEMPORAL | PMEM2_F_MEM_NODRAIN);
	stream->persist(span, SPAN_ENTRY_METADATA_SIZE);
}

void pmemstream_span_create_region(struct pmemstream *stream, uint64_t offset, size_t size)
{
	pmemstream_span_bytes *span = pmemstream_offset_to_span_ptr(stream, offset);
	assert((size & PMEMSTREAM_SPAN_TYPE_MASK) == 0);
	span[0] = size | PMEMSTREAM_SPAN_REGION;

	stream->persist(span, SPAN_REGION_METADATA_SIZE);
}

uint64_t pmemstream_get_span_size(pmemstream_span_bytes *span)
{
	return span[0] & PMEMSTREAM_SPAN_EXTRA_MASK;
}

enum pmemstream_span_type pmemstream_get_span_type(pmemstream_span_bytes *span)
{
	return span[0] & PMEMSTREAM_SPAN_TYPE_MASK;
}

struct pmemstream_span_runtime pmemstream_span_get_empty_runtime(struct pmemstream *stream, uint64_t offset)
{
	pmemstream_span_bytes *span = pmemstream_offset_to_span_ptr(stream, offset);
	struct pmemstream_span_runtime srt;

	assert(pmemstream_get_span_type(span) == PMEMSTREAM_SPAN_EMPTY);

	srt.type = PMEMSTREAM_SPAN_EMPTY;
	srt.empty.size = pmemstream_get_span_size(span);
	srt.data_offset = offset + SPAN_EMPTY_METADATA_SIZE;
	srt.total_size = ALIGN_UP(srt.empty.size + SPAN_EMPTY_METADATA_SIZE, sizeof(pmemstream_span_bytes));

	return srt;
}

struct pmemstream_span_runtime pmemstream_span_get_entry_runtime(struct pmemstream *stream, uint64_t offset)
{
	pmemstream_span_bytes *span = pmemstream_offset_to_span_ptr(stream, offset);
	struct pmemstream_span_runtime srt;

	assert(pmemstream_get_span_type(span) == PMEMSTREAM_SPAN_ENTRY);

	srt.type = PMEMSTREAM_SPAN_ENTRY;
	srt.entry.size = pmemstream_get_span_size(span);
	srt.entry.popcount = span[1];
	srt.data_offset = offset + SPAN_ENTRY_METADATA_SIZE;
	srt.total_size = ALIGN_UP(srt.entry.size + SPAN_ENTRY_METADATA_SIZE, sizeof(pmemstream_span_bytes));

	return srt;
}

struct pmemstream_span_runtime pmemstream_span_get_region_runtime(struct pmemstream *stream, uint64_t offset)
{
	pmemstream_span_bytes *span = pmemstream_offset_to_span_ptr(stream, offset);
	struct pmemstream_span_runtime srt;

	assert(pmemstream_get_span_type(span) == PMEMSTREAM_SPAN_REGION);

	srt.type = PMEMSTREAM_SPAN_REGION;
	srt.region.size = pmemstream_get_span_size(span);
	srt.data_offset = offset + SPAN_REGION_METADATA_SIZE;
	srt.total_size = ALIGN_UP(srt.region.size + SPAN_REGION_METADATA_SIZE, sizeof(pmemstream_span_bytes));

	return srt;
}

struct pmemstream_span_runtime pmemstream_span_get_runtime(struct pmemstream *stream, uint64_t offset)
{
	pmemstream_span_bytes *span = pmemstream_offset_to_span_ptr(stream, offset);
	struct pmemstream_span_runtime srt;

	switch (pmemstream_get_span_type(span)) {
		case PMEMSTREAM_SPAN_EMPTY:
			srt = pmemstream_span_get_empty_runtime(stream, offset);
			break;
		case PMEMSTREAM_SPAN_ENTRY:
			srt = pmemstream_span_get_entry_runtime(stream, offset);
			break;
		case PMEMSTREAM_SPAN_REGION:
			srt = pmemstream_span_get_region_runtime(stream, offset);
			break;
		default:
			abort();
	}

	return srt;
}
