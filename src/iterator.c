// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include "iterator.h"
#include "common/util.h"
#include "libpmemstream_internal.h"
#include "region.h"

#include <assert.h>

int pmemstream_region_iterator_new(struct pmemstream_region_iterator **iterator, struct pmemstream *stream)
{
	struct pmemstream_region_iterator *iter = malloc(sizeof(*iter));
	if (!iter) {
		return -1;
	}

	iter->stream = stream;
	iter->region.offset = 0;

	*iterator = iter;

	return 0;
}

int pmemstream_region_iterator_next(struct pmemstream_region_iterator *it, struct pmemstream_region *region)
{
	struct span_runtime srt;

	while (it->region.offset < it->stream->usable_size) {
		srt = span_get_runtime(it->stream, it->region.offset);

		if (srt.type == SPAN_REGION) {
			*region = it->region;
			it->region.offset += srt.total_size;
			return 0;
		}

		/* if there are no more regions we should expect an empty span */
		assert(srt.type == SPAN_EMPTY);
		it->region.offset += srt.total_size;
	}

	return -1;
}

void pmemstream_region_iterator_delete(struct pmemstream_region_iterator **iterator)
{
	struct pmemstream_region_iterator *iter = *iterator;

	free(iter);
	*iterator = NULL;
}

int entry_iterator_initialize(struct pmemstream_entry_iterator *iterator, struct pmemstream *stream,
			      struct pmemstream_region region)
{
	struct span_runtime region_srt = span_get_region_runtime(stream, region.offset);
	struct pmemstream_entry_iterator iter;
	iter.offset = region_srt.data_offset;
	iter.region = region;
	iter.stream = stream;

	int ret = region_contexts_map_get_or_create(stream->region_contexts_map, region, &iter.region_context);
	if (ret) {
		return ret;
	}

	*iterator = iter;

	return 0;
}

int pmemstream_entry_iterator_new(struct pmemstream_entry_iterator **iterator, struct pmemstream *stream,
				  struct pmemstream_region region)
{
	struct pmemstream_entry_iterator *iter = malloc(sizeof(*iter));
	if (!iter) {
		return -1;
	}

	int ret = entry_iterator_initialize(iter, stream, region);
	if (ret) {
		goto err;
	}

	*iterator = iter;

	return 0;

err:
	free(iter);
	return ret;
}

static int validate_entry(struct pmemstream *stream, struct pmemstream_entry entry, struct pmemstream_region region)
{
	struct span_runtime srt = span_get_runtime(stream, entry.offset);
	struct span_runtime region_rt = span_get_runtime(stream, region.offset);

	size_t region_end_offset = region.offset + region_rt.total_size;
	size_t remaining_size = region_end_offset - entry.offset;

	void *entry_data = pmemstream_offset_to_ptr(stream, srt.data_offset);
	if (srt.type == SPAN_ENTRY && srt.total_size <= remaining_size &&
	    util_popcount_memory(entry_data, srt.entry.size) == srt.entry.popcount) {
		return 0;
	}
	return -1;
}

/* Advances entry iterator by one. Verifies entry integrity and sets append_offset is end of data is found. */
int pmemstream_entry_iterator_next(struct pmemstream_entry_iterator *iterator, struct pmemstream_region *region,
				   struct pmemstream_entry *user_entry)
{
	struct span_runtime srt = span_get_runtime(iterator->stream, iterator->offset);
	struct span_runtime region_srt = span_get_region_runtime(iterator->stream, iterator->region.offset);
	struct pmemstream_entry entry;
	entry.offset = iterator->offset;

	// XXX: add test for NULL entry
	if (user_entry) {
		*user_entry = entry;
	}
	if (region) {
		*region = iterator->region;
	}

	/* Make sure that we didn't go beyond region. */
	if (iterator->offset >= iterator->region.offset + region_srt.total_size) {
		return -1;
	}

	int append_offset_initialized = region_is_append_offset_initialized(iterator->region_context);
	if (append_offset_initialized && srt.type == SPAN_EMPTY) {
		/* If we found last entry and append_offset is already initialized, just return -1. */
		return -1;
	}

	if (validate_entry(iterator->stream, entry, iterator->region) < 0) {
		if (!append_offset_initialized) {
			/* If append_offset was not set yet, validate that entry is correct. If entry is not valid, set
			 * append_offset to point to that entry. */
			region_initialize_append_offset(iterator->stream, iterator->region, iterator->region_context,
							entry);
		}

		return -1;
	}

	/*
	 * Verify that all metadata and data fits inside the region - this should not fail unless stream was corrupted.
	 */
	assert(entry.offset + srt.total_size <= iterator->region.offset + region_srt.total_size);

	iterator->offset += srt.total_size;

	return 0;
}

void pmemstream_entry_iterator_delete(struct pmemstream_entry_iterator **iterator)
{
	struct pmemstream_entry_iterator *iter = *iterator;

	free(iter);
	*iterator = NULL;
}
