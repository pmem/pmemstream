// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include "iterator.h"
#include "common/util.h"
#include "libpmemstream_internal.h"
#include "region.h"

#include <assert.h>
#include <stdbool.h>
#include <string.h>

int pmemstream_region_iterator_new(struct pmemstream_region_iterator **iterator, struct pmemstream *stream)
{
	if (!stream || !iterator) {
		return -1;
	}

	struct pmemstream_region_iterator *iter = malloc(sizeof(*iter));
	if (!iter) {
		return -1;
	}

	*(struct pmemstream **)&iter->stream = stream;

	/* XXX: lock */
	iter->region.offset = SLIST_INVALID_OFFSET;
	*iterator = iter;

	return 0;
}

int pmemstream_region_iterator_is_valid(struct pmemstream_region_iterator *iterator)
{
	if (!iterator) {
		return -1;
	}

	if (iterator->region.offset == SLIST_INVALID_OFFSET) {
		return -1;
	}

	return 0;
}

void pmemstream_region_iterator_seek_first(struct pmemstream_region_iterator *iterator)
{
	if (!iterator)
		return;
	iterator->region.offset = iterator->stream->header->region_allocator_header.allocated_list.head;
}

void pmemstream_region_iterator_next(struct pmemstream_region_iterator *iterator)
{
	if (!iterator)
		return;
	iterator->region.offset = SLIST_NEXT(struct span_region, &iterator->stream->data, iterator->region.offset,
					     allocator_entry_metadata.next_allocated);
}

struct pmemstream_region pmemstream_region_iterator_get(struct pmemstream_region_iterator *iterator)
{
	int is_valid = pmemstream_region_iterator_is_valid(iterator);
	if (is_valid != 0) {
		struct pmemstream_region region = {.offset = SLIST_INVALID_OFFSET};
		return region;
	}

	return iterator->region;
}

void pmemstream_region_iterator_delete(struct pmemstream_region_iterator **iterator)
{
	if (!iterator) {
		return;
	}
	if (!(*iterator)) {
		return;
	}

	struct pmemstream_region_iterator *iter = *iterator;

	free(iter);
	*iterator = NULL;
}

int entry_iterator_initialize(struct pmemstream_entry_iterator *iterator, struct pmemstream *stream,
			      struct pmemstream_region region, bool perform_recovery)
{
	int ret = pmemstream_validate_stream_and_offset(stream, region.offset);
	if (ret) {
		return ret;
	}

	assert(span_get_type(span_offset_to_span_ptr(&stream->data, region.offset)) == SPAN_REGION);

	struct pmemstream_region_runtime *region_rt;
	ret = region_runtimes_map_get_or_create(stream->region_runtimes_map, region, &region_rt);
	if (ret) {
		return ret;
	}

	struct pmemstream_entry_iterator iter = {.stream = stream,
						 .offset = PMEMSTREAM_INVALID_OFFSET,
						 .region = region,
						 .region_runtime = region_rt,
						 .perform_recovery = perform_recovery};
	memcpy(iterator, &iter, sizeof(struct pmemstream_entry_iterator));

	return 0;
}

int pmemstream_entry_iterator_new(struct pmemstream_entry_iterator **iterator, struct pmemstream *stream,
				  struct pmemstream_region region)
{
	if (!iterator) {
		return -1;
	}

	struct pmemstream_entry_iterator *iter = malloc(sizeof(*iter));
	if (!iter) {
		return -1;
	}

	int ret = entry_iterator_initialize(iter, stream, region, true);
	if (ret) {
		goto err;
	}

	*iterator = iter;

	return 0;

err:
	free(iter);
	return ret;
}

static bool pmemstream_entry_iterator_offset_is_inside_region(struct pmemstream_entry_iterator *iterator)
{
	/* XXX: we should update region_free to change 'region_span' into 'empty_span' and add check here
	 * if the iterator is not on freed region; right now it would be costly to iterate over "free regions list"
	 */
	const struct span_base *span_base = span_offset_to_span_ptr(&iterator->stream->data, iterator->region.offset);
	uint64_t region_end_offset = iterator->region.offset + span_get_total_size(span_base);
	return iterator->offset >= iterator->region.offset && iterator->offset <= region_end_offset;
}

int pmemstream_entry_iterator_is_valid(struct pmemstream_entry_iterator *iterator)
{
	if (!iterator) {
		return -1;
	}

	if (iterator->offset == PMEMSTREAM_INVALID_OFFSET) {
		return -1;
	}

	if (!pmemstream_entry_iterator_offset_is_inside_region(iterator)) {
		return -1;
	}

	if (check_entry_consistency(iterator)) {
		return 0;
	}
	return -1;
}

static void pmemstream_entry_iterator_advance(struct pmemstream_entry_iterator *iterator)
{
	assert(pmemstream_entry_iterator_offset_is_inside_region(iterator));

	const struct span_base *span_base = span_offset_to_span_ptr(&iterator->stream->data, iterator->offset);
	iterator->offset += span_get_total_size(span_base);
}

/* Advances entry iterator by one. Verifies entry integrity and initializes region runtime if end of data is found. */
void pmemstream_entry_iterator_next(struct pmemstream_entry_iterator *iterator)
{
	if (!iterator) {
		return;
	}

	if (iterator->offset == PMEMSTREAM_INVALID_OFFSET) {
		return;
	}

	assert(pmemstream_entry_iterator_is_valid(iterator) == 0);

	struct pmemstream_entry_iterator tmp_iterator = *iterator;
	pmemstream_entry_iterator_advance(&tmp_iterator);
	if (pmemstream_entry_iterator_offset_is_inside_region(&tmp_iterator)) {
		pmemstream_entry_iterator_advance(iterator);
		/* Verify that all metadata and data fits inside the region after iterator
		 * increment - this check should not fail unless stream was corrupted. */
		assert(pmemstream_entry_iterator_offset_is_inside_region(iterator));
	}
	check_entry_and_maybe_recover_region(iterator);
}

void pmemstream_entry_iterator_seek_first(struct pmemstream_entry_iterator *iterator)
{
	if (!iterator) {
		return;
	}
	struct pmemstream_entry_iterator tmp_iterator = *iterator;

	tmp_iterator.offset = region_first_entry_offset(iterator->region);
	if (!check_entry_and_maybe_recover_region(&tmp_iterator)) {
		iterator->offset = PMEMSTREAM_INVALID_OFFSET;
		return;
	}
	iterator->offset = tmp_iterator.offset;
	assert(pmemstream_entry_iterator_is_valid(iterator) == 0);
}

struct pmemstream_entry pmemstream_entry_iterator_get(struct pmemstream_entry_iterator *iterator)
{
	struct pmemstream_entry entry;
	if (!iterator) {
		entry.offset = PMEMSTREAM_INVALID_OFFSET;
	} else {
		entry.offset = iterator->offset;
	}
	return entry;
}

void pmemstream_entry_iterator_delete(struct pmemstream_entry_iterator **iterator)
{
	if (!iterator) {
		return;
	}
	if (!(*iterator)) {
		return;
	}

	struct pmemstream_entry_iterator *iter = *iterator;

	free(iter);
	*iterator = NULL;
}
