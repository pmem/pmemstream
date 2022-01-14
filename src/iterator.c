// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include "iterator.h"
#include "common/util.h"
#include "libpmemstream_internal.h"
#include "region.h"

#include <assert.h>
#include <string.h>

int pmemstream_region_iterator_new(struct pmemstream_region_iterator **iterator, struct pmemstream *stream)
{
	struct pmemstream_region_iterator *iter = malloc(sizeof(*iter));
	if (!iter) {
		return -1;
	}

	*(struct pmemstream **)&iter->stream = stream;
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
	struct pmemstream_region_runtime *region_rt;

	int ret = region_runtimes_map_get_or_create(stream->region_runtimes_map, region, &region_rt);
	if (ret) {
		return ret;
	}

	struct pmemstream_entry_iterator iter = {
		.stream = stream, .offset = region_srt.data_offset, .region = region, .region_runtime = region_rt};
	memcpy(iterator, &iter, sizeof(struct pmemstream_entry_iterator));

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

static int validate_entry(const struct pmemstream *stream, struct pmemstream_entry entry)
{
	struct span_runtime srt = span_get_runtime(stream, entry.offset);
	const void *entry_data = pmemstream_offset_to_ptr(stream, srt.data_offset);
	if (srt.type == SPAN_ENTRY && util_popcount_memory(entry_data, srt.entry.size) == srt.entry.popcount) {
		return 0;
	}
	return -1;
}

/* Advances entry iterator by one. Verifies entry integrity and sets append_offset if end of data is found. */
int pmemstream_entry_iterator_next(struct pmemstream_entry_iterator *iterator, struct pmemstream_region *region,
				   struct pmemstream_entry *user_entry)
{
	struct span_runtime region_srt = span_get_region_runtime(iterator->stream, iterator->region.offset);
	struct pmemstream_entry entry = {.offset = iterator->offset};
	const uint64_t region_end_offset = iterator->region.offset + region_srt.total_size;

	// XXX: add test for NULL entry
	if (user_entry) {
		*user_entry = entry;
	}
	if (region) {
		*region = iterator->region;
	}

	int initialized = region_is_runtime_initialized(iterator->region_runtime);
	if (initialized) {
		/* Make sure that we didn't go beyond committed entires. */
		uint64_t committed_offset =
			__atomic_load_n(&iterator->region_runtime->committed_offset, __ATOMIC_ACQUIRE);
		if (iterator->offset >= committed_offset) {
			return -1;
		}

		/* committed_offset (and hence iterator->offset) should not be bigger than end of region offset. */
		assert(committed_offset <= region_end_offset);
		/* Region is already recovered, and we did not encounter end of the data yet - span must be a valid
		 * entry */
		assert(validate_entry(iterator->stream, entry) == 0);
	} else if (iterator->offset >= region_end_offset || validate_entry(iterator->stream, entry) < 0) {
		/* If we arrived at end of the region or entry is not valid. */
		region_runtime_initialize(iterator->region_runtime, entry);
		return -1;
	}

	/*
	 * Verify that all metadata and data fits inside the region - this should not fail unless stream was corrupted.
	 */
	struct span_runtime srt = span_get_runtime(iterator->stream, iterator->offset);
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
