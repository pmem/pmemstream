// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include "iterator.h"
#include "common/util.h"
#include "libpmemstream_internal.h"
#include "region.h"

#include <assert.h>
#include <stdbool.h>

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

	int ret = region_runtimes_map_get_or_create(stream->region_runtimes_map, region, &iter.region_runtime);
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

static int validate_entry(struct pmemstream *stream, struct pmemstream_entry entry)
{
	struct span_runtime srt = span_get_runtime(stream, entry.offset);
	void *entry_data = pmemstream_offset_to_ptr(stream, srt.data_offset);
	if (srt.type == SPAN_ENTRY && util_popcount_memory(entry_data, srt.entry.size) == srt.entry.popcount) {
		return 0;
	}
	return -1;
}

static bool pmemstream_entry_iterator_offset_is_inside_region(struct pmemstream_entry_iterator *iterator)
{
	struct span_runtime region_srt = span_get_region_runtime(iterator->stream, iterator->region.offset);
	uint64_t region_end_offset = iterator->region.offset + region_srt.total_size;
	return iterator->offset >= iterator->region.offset && iterator->offset <= region_end_offset;
}

/* Precondition: region_runtime is initialized. */
static bool pmemstream_entry_iterator_offset_is_below_commited(struct pmemstream_entry_iterator *iterator)
{
	assert(region_runtime_is_initialized(iterator->region_runtime));

	/* Make sure that we didn't go beyond commited entires. */
	uint64_t commited_offset = __atomic_load_n(&iterator->region_runtime->commited_offset, __ATOMIC_ACQUIRE);
	if (iterator->offset >= commited_offset) {
		return false;
	}

#ifndef NDEBUG
	assert(pmemstream_entry_iterator_offset_is_inside_region(iterator));
	/* Region is already recovered, and we did not encounter end of the data yet - span must be a valid
	 * entry */
	struct pmemstream_entry entry = {.offset = iterator->offset};
	assert(validate_entry(iterator->stream, entry) == 0);
#endif

	return true;
}

static bool pmemstream_entry_iterator_offset_at_valid_entry(struct pmemstream_entry_iterator *iterator)
{
	assert(pmemstream_entry_iterator_offset_is_inside_region(iterator));

	struct span_runtime region_srt = span_get_region_runtime(iterator->stream, iterator->region.offset);
	uint64_t region_end_offset = iterator->region.offset + region_srt.total_size;
	struct pmemstream_entry entry = {.offset = iterator->offset};

	return iterator->offset < region_end_offset && validate_entry(iterator->stream, entry) == 0;
}

static void pmemstream_entry_iterator_advance(struct pmemstream_entry_iterator *iterator)
{
	/* Verify that all metadata and data fits inside the region before and after iterator
	 * increment - those checks should not fail unless stream was corrupted. */
	assert(pmemstream_entry_iterator_offset_is_inside_region(iterator));

	struct span_runtime srt = span_get_runtime(iterator->stream, iterator->offset);
	iterator->offset += srt.total_size;

	assert(pmemstream_entry_iterator_offset_is_inside_region(iterator));
}

static int pmemstream_entry_iterator_next_when_region_initialized(struct pmemstream_entry_iterator *iterator)
{
	if (pmemstream_entry_iterator_offset_is_below_commited(iterator)) {
		pmemstream_entry_iterator_advance(iterator);
		return 0;
	} else {
		return -1;
	}
}

static int pmemstream_entry_iterator_next_when_region_not_initialized(struct pmemstream_entry_iterator *iterator)
{
	struct pmemstream_entry entry = {.offset = iterator->offset};

	if (pmemstream_entry_iterator_offset_at_valid_entry(iterator)) {
		pmemstream_entry_iterator_advance(iterator);
		return 0;
	} else {
		/* Lazy initialization of region - now we have all the information (entry offset) to do it. */
		region_runtime_initialize(iterator->region_runtime, entry);
		return -1;
	}
}

/* Advances entry iterator by one. Verifies entry integrity and initializes region runtime if end of data is found. */
int pmemstream_entry_iterator_next(struct pmemstream_entry_iterator *iterator, struct pmemstream_region *region,
				   struct pmemstream_entry *user_entry)
{
	// XXX: add test for NULL entry
	if (user_entry) {
		user_entry->offset = iterator->offset;
	}
	if (region) {
		*region = iterator->region;
	}

	// XXX: cache in iterator to avoid reading from global var???? - write to TODOLIST_ with possible optimizations
	if (region_runtime_is_initialized(iterator->region_runtime)) {
		return pmemstream_entry_iterator_next_when_region_initialized(iterator);
	} else {
		return pmemstream_entry_iterator_next_when_region_not_initialized(iterator);
	}
}

void pmemstream_entry_iterator_delete(struct pmemstream_entry_iterator **iterator)
{
	struct pmemstream_entry_iterator *iter = *iterator;

	free(iter);
	*iterator = NULL;
}
