// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include "span.h"

#include <assert.h>

#include "libpmemstream_internal.h"

const span_bytes *span_offset_to_span_ptr(const struct pmemstream *stream, uint64_t offset)
{
	assert(offset % sizeof(span_bytes) == 0);

	return (const span_bytes *)pmemstream_offset_to_ptr(stream, offset);
}

/* Creates empty span at given offset.
 * It sets empty's type and size, and zeros out the whole span.
 */
void span_create_empty(struct pmemstream *stream, uint64_t offset, size_t size)
{
	span_bytes *span = (span_bytes *)span_offset_to_span_ptr(stream, offset);
	assert((size & SPAN_TYPE_MASK) == 0);
	span[0] = size | SPAN_EMPTY;

	void *dest = ((uint8_t *)span) + SPAN_EMPTY_METADATA_SIZE;
	stream->memset(dest, 0, size, PMEM2_F_MEM_NONTEMPORAL | PMEM2_F_MEM_NODRAIN);
	stream->persist(span, SPAN_EMPTY_METADATA_SIZE);
}

/* Internal helper for span_create_entry. */
static void span_create_entry_internal(struct pmemstream *stream, uint64_t offset, size_t data_size, size_t popcount,
				       size_t flush_size)
{
	span_bytes *span = (span_bytes *)span_offset_to_span_ptr(stream, offset);
	assert((data_size & SPAN_TYPE_MASK) == 0);

	// XXX - use variadic mempcy to store data and metadata at once
	span[0] = data_size | SPAN_ENTRY;
	span[1] = popcount;

	stream->persist(span, flush_size);
}

/* Creates entry span at given offset.
 * It sets entry's metadata: type, size of the data and popcount.
 * It flushes metadata along with the data (of given 'data_size'), which are stored in the spans following metadata.
 */
void span_create_entry(struct pmemstream *stream, uint64_t offset, size_t data_size, size_t popcount)
{
	span_create_entry_internal(stream, offset, data_size, popcount, SPAN_ENTRY_METADATA_SIZE + data_size);
}

/* Creates entry span at given offset.
 * It sets entry's metadata: type, size of the data and popcount.
 * It flushes only the metadata.
 */
void span_create_entry_no_flush_data(struct pmemstream *stream, uint64_t offset, size_t data_size, size_t popcount)
{
	span_create_entry_internal(stream, offset, data_size, popcount, SPAN_ENTRY_METADATA_SIZE);
}

/* Creates region span at given offset.
 * It only sets region's type and size.
 */
void span_create_region(struct pmemstream *stream, uint64_t offset, size_t size)
{
	span_bytes *span = (span_bytes *)span_offset_to_span_ptr(stream, offset);
	assert((size & SPAN_TYPE_MASK) == 0);
	span[0] = size | SPAN_REGION;

	stream->persist(span, SPAN_REGION_METADATA_SIZE);
}

uint64_t span_get_size(const span_bytes *span)
{
	return span[0] & SPAN_EXTRA_MASK;
}

enum span_type span_get_type(const span_bytes *span)
{
	return span[0] & SPAN_TYPE_MASK;
}

struct span_runtime span_get_empty_runtime(const struct pmemstream *stream, uint64_t offset)
{
	const span_bytes *span = span_offset_to_span_ptr(stream, offset);
	struct span_runtime srt;

	assert(span_get_type(span) == SPAN_EMPTY);

	srt.type = SPAN_EMPTY;
	srt.empty.size = span_get_size(span);
	srt.data_offset = offset + SPAN_EMPTY_METADATA_SIZE;
	srt.total_size = ALIGN_UP(srt.empty.size + SPAN_EMPTY_METADATA_SIZE, sizeof(span_bytes));

	return srt;
}

struct span_runtime span_get_entry_runtime(const struct pmemstream *stream, uint64_t offset)
{
	const span_bytes *span = span_offset_to_span_ptr(stream, offset);
	struct span_runtime srt;

	assert(span_get_type(span) == SPAN_ENTRY);

	srt.type = SPAN_ENTRY;
	srt.entry.size = span_get_size(span);
	srt.entry.popcount = span[1];
	srt.data_offset = offset + SPAN_ENTRY_METADATA_SIZE;
	srt.total_size = ALIGN_UP(srt.entry.size + SPAN_ENTRY_METADATA_SIZE, 64ULL);

	return srt;
}

struct span_runtime span_get_region_runtime(const struct pmemstream *stream, uint64_t offset)
{
	const span_bytes *span = span_offset_to_span_ptr(stream, offset);
	struct span_runtime srt;

	assert(span_get_type(span) == SPAN_REGION);

	srt.type = SPAN_REGION;
	srt.region.size = span_get_size(span);
	srt.data_offset = offset + SPAN_REGION_METADATA_SIZE;
	srt.total_size = ALIGN_UP(srt.region.size + SPAN_REGION_METADATA_SIZE, sizeof(span_bytes));

	return srt;
}

struct span_runtime span_get_runtime(const struct pmemstream *stream, uint64_t offset)
{
	const span_bytes *span = span_offset_to_span_ptr(stream, offset);
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
