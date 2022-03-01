// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include "ringbuffer.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define ENTRY_NOT_INITIALIZED UINT64_MAX

#define MOVED_TO_NEXT_REGION 2

static struct ringbuffer_position ringbuffer_position_new(struct pmemstream *stream)
{
	struct pmemstream_region_iterator *region_iterator = NULL;

	pmemstream_region_iterator_new(&region_iterator, stream);

	struct pmemstream_region first_region;
	struct pmemstream_entry first_entry;

	pmemstream_region_iterator_next(region_iterator, &first_region);
	struct pmemstream_entry_iterator *entry_iterator = NULL;
	int ret = pmemstream_entry_iterator_new(&entry_iterator, stream, first_region);
	(void)ret;
	assert(ret == 0);
	first_entry.offset = ENTRY_NOT_INITIALIZED;

	struct ringbuffer_position new_position = {.stream = stream,
						   .region_iterator = region_iterator,
						   .current_region = first_region,
						   .entry_iterator = entry_iterator,
						   .current_entry = first_entry};
	return new_position;
};

static int ringbuffer_position_next(struct ringbuffer_position *position)
{
	struct pmemstream_entry entry;
	struct pmemstream_region region;

	int entry_ret = pmemstream_entry_iterator_next(position->entry_iterator, NULL, &entry);
	if (entry_ret != 0) {
		return_on_failure(pmemstream_region_iterator_next(position->region_iterator, &region));
		position->current_region = region;
		pmemstream_entry_iterator_delete(&position->entry_iterator);
		return_on_failure(pmemstream_entry_iterator_new(&position->entry_iterator, position->stream, region));
		return_on_failure(pmemstream_entry_iterator_next(position->entry_iterator, NULL, &entry));
	}
	__atomic_store_n(&position->current_entry.offset, entry.offset, __ATOMIC_RELAXED);

	if (entry_ret != 0) {
		return MOVED_TO_NEXT_REGION;
	}

	return 0;
}

static int ringbuffer_position_next_and_reallocate_region(struct ringbuffer_runtime *runtime,
							  struct ringbuffer_position *position)
{
	struct pmemstream_region previous_region = position->current_region;
	int ret = ringbuffer_position_next(position);
	if (ret == MOVED_TO_NEXT_REGION) {
		printf("Free region: %ld\n", previous_region.offset);
		pmemstream_region_free(position->stream, previous_region);

		struct pmemstream_region new_region;
		return_on_failure(pmemstream_region_allocate(runtime->stream, runtime->region_size, &new_region));
		printf("Allocate region: %ld\n", new_region.offset);
	}
	return ret;
}

static bool ringbuffer_position_equal(struct ringbuffer_position *lhs, struct ringbuffer_position *rhs)
{
	uint64_t lhs_offset = __atomic_load_n(&lhs->current_entry.offset, __ATOMIC_RELAXED);
	uint64_t rhs_offset = __atomic_load_n(&rhs->current_entry.offset, __ATOMIC_RELAXED);

	if (lhs_offset == rhs_offset) {
		return true;
	}
	return false;
}

static void ringbuffer_position_delete(struct ringbuffer_position *position)
{
	pmemstream_region_iterator_delete(&position->region_iterator);
	pmemstream_entry_iterator_delete(&position->entry_iterator);
}

struct ringbuffer_runtime ringbuffer_runtime_new(struct pmemstream *stream, size_t region_size)
{
	struct ringbuffer_runtime runtime = {.stream = stream,
					     .region_size = region_size,
					     .producer_position = ringbuffer_position_new(stream),
					     .consumer_position = ringbuffer_position_new(stream)};
	return runtime;
}

static bool ringbuffer_is_empty(struct ringbuffer_runtime *runtime)
{
	uint64_t producer_offset = __atomic_load_n(&runtime->producer_position.current_entry.offset, __ATOMIC_RELAXED);
	if (producer_offset == ENTRY_NOT_INITIALIZED) {
		return true;
	}
	return false;
}

size_t ringbuffer_runtime_consume(struct ringbuffer_runtime *runtime, void *data)
{
	if (ringbuffer_is_empty(runtime)) {
		return 0;
	}

	if (ringbuffer_position_equal(&runtime->consumer_position, &runtime->producer_position)) {
		return 0;
	}

	if (ringbuffer_position_next_and_reallocate_region(runtime, &runtime->consumer_position) != 0) {
		return 0;
	}

	struct pmemstream_entry entry = runtime->consumer_position.current_entry;

	const void *entry_data = pmemstream_entry_data(runtime->stream, runtime->consumer_position.current_entry);

	if (entry_data == NULL) {
		return 0;
	}
	const size_t entry_length = pmemstream_entry_length(runtime->stream, entry);

	memcpy(data, entry_data, entry_length);

	return entry_length;
}

int ringbuffer_runtime_produce(struct ringbuffer_runtime *runtime, const void *data, size_t size)
{
	struct pmemstream_entry new_entry;

	int ret_append = pmemstream_append(runtime->stream, runtime->producer_position.current_region, NULL, data, size,
					   &new_entry);

	return_on_failure(ringbuffer_position_next(&runtime->producer_position));

	return ret_append;
}

void ringbuffer_runtime_delete(struct ringbuffer_runtime *runtime)
{
	ringbuffer_position_delete(&runtime->producer_position);
	ringbuffer_position_delete(&runtime->consumer_position);
}
