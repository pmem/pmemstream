// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

#include "iterator.h"
#include "common/util.h"

#include "libpmemstream_internal.h"
#include "region.h"
#include <assert.h>

int pmemstream_region_iterator_new(struct pmemstream_region_iterator **iterator, struct pmemstream *stream)
{
	struct pmemstream_region_iterator *iter = malloc(sizeof(*iter));
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

	int ret = entry_iterator_initialize(iter, stream, region);
	if (ret) {
		free(iter);
		return ret;
	}

	*iterator = iter;

	return 0;
}

static int validate_entry(struct pmemstream *stream, struct pmemstream_entry entry)
{
	struct span_runtime srt = span_get_runtime(stream, entry.offset);
	void *entry_data = pmemstream_offset_to_ptr(stream, srt.data_offset);
	if (srt.type == SPAN_ENTRY && util_popcount_memory(entry_data, srt.entry.size) == srt.entry.popcount) {
		return 0;
	}
	return -1;
}

/* Advances entry iterator by one. Verifies entry integrity and recovers the region if necessary. */
int pmemstream_entry_iterator_next(struct pmemstream_entry_iterator *iter, struct pmemstream_region *region,
				   struct pmemstream_entry *user_entry)
{
	struct span_runtime srt = span_get_runtime(iter->stream, iter->offset);
	struct span_runtime region_srt = span_get_region_runtime(iter->stream, iter->region.offset);
	struct pmemstream_entry entry;
	entry.offset = iter->offset;

	// XXX: add test for NULL entry
	if (user_entry) {
		*user_entry = entry;
	}
	if (region) {
		*region = iter->region;
	}

	/* Make sure that we didn't go beyond region. */
	if (iter->offset >= iter->region.offset + region_srt.total_size) {
		return -1;
	}

	iter->offset += srt.total_size;

	/*
	 * Verify that all metadata and data fits inside the region - this should not fail unless stream was corrupted.
	 */
	assert(entry.offset + srt.total_size <= iter->region.offset + region_srt.total_size);

	int region_recovered = region_is_recovered(iter->region_context);

	if (region_recovered && srt.type == SPAN_EMPTY) {
		/* If we found last entry and region is already recovered, just return -1. */
		return -1;
	} else if (!region_recovered && validate_entry(iter->stream, entry) < 0) {
		/* If region was not yet recovered, validate that entry is correct. If there is any problem, recover the
		 * region. */
		region_recover(iter->stream, iter->region, iter->region_context, entry);
		return -1;
	}

	/* Region is already recovered, and we did not encounter end of the data yet - span must be a valid entry */
	assert(validate_entry(iter->stream, entry) == 0);

	return 0;
}

void pmemstream_entry_iterator_delete(struct pmemstream_entry_iterator **iterator)
{
	struct pmemstream_entry_iterator *iter = *iterator;

	free(iter);
	*iterator = NULL;
}
