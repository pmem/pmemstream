// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include "ringbuffer.h"
#include <string.h>

static struct ringbuffer_position ringbuffer_position_new(struct pmemstream *stream)
{
	struct pmemstream_region_iterator *region_iterator = NULL;

	pmemstream_region_iterator_new(&region_iterator, stream);

	struct pmemstream_region first_region;
	struct pmemstream_entry first_entry;

	pmemstream_region_iterator_next(region_iterator, &first_region);
	struct pmemstream_entry_iterator *entry_iterator = NULL;
	pmemstream_entry_iterator_new(&entry_iterator, stream, first_region);
	pmemstream_entry_iterator_next(entry_iterator, NULL, &first_entry);

	struct ringbuffer_position new_position = {.stream = stream,
						   .region_iterator = region_iterator,
						   .current_region = first_region,
						   .entry_iterator = entry_iterator,
						   .current_entry = first_entry};
	return new_position;
};

// XXX: handle errors
static int ringbuffer_position_next(struct ringbuffer_position *position)
{
	struct pmemstream_entry entry;
	struct pmemstream_region region;

	int entry_ret = pmemstream_entry_iterator_next(position->entry_iterator, NULL, &entry);
	if (entry_ret != 0) {
		int region_ret = pmemstream_region_iterator_next(position->region_iterator, &region);
		if (region_ret != 0) {
			return region_ret;
		}
		position->current_region = region;
		pmemstream_entry_iterator_delete(&position->entry_iterator);
		pmemstream_entry_iterator_new(&position->entry_iterator, position->stream, region);
		pmemstream_entry_iterator_next(position->entry_iterator, NULL, &entry);
	}
	position->current_entry = entry;
	return 0;
}

static bool ringbuffer_position_equal(struct ringbuffer_position *lhs, struct ringbuffer_position *rhs)
{
	/* XXX: Those offsets are in public API, but it's little bit leaky
	 * abstraction */
	if (lhs->current_region.offset == rhs->current_region.offset) {
		if (lhs->current_entry.offset == rhs->current_entry.offset) {
			return true;
		}
	}
	return false;
	/* XXX: I would rather do it this way, but need add getters for
	 * iterators
	 *return pmemstream_entry_iterator_equal(lhs->entry_iterator, rhs->entry_iterator);
	 */
}

static void ringbuffer_position_delete(struct ringbuffer_position *position)
{
	pmemstream_region_iterator_delete(&position->region_iterator);
	pmemstream_entry_iterator_delete(&position->entry_iterator);
}

/* Do it in separate worker thread */
static int ringbuffer_runtime_reallocate_region(struct ringbuffer_runtime *runtime)
{
	struct pmemstream_region_iterator *it;
	pmemstream_region_iterator_new(&it, runtime->stream);

	struct pmemstream_region region;
	pmemstream_region_iterator_next(it, &region);

	if (pmemstream_region_iterator_equal(runtime->consumer_position.region_iterator, it)) {
		return -1;
	} else {
		struct pmemstream_region dummy_region;
		pmemstream_region_free(runtime->stream, region);
		int ret = pmemstream_region_allocate(runtime->stream, runtime->region_size, &dummy_region);
		if (ret != 0) {
			return ret;
		}
	}

	pmemstream_region_iterator_delete(&it);
	return 0;
}

struct ringbuffer_runtime ringbuffer_runtime_new(struct pmemstream *stream, size_t region_size)
{
	struct ringbuffer_runtime runtime = {.stream = stream,
					     .region_size = region_size,
					     .producer_position = ringbuffer_position_new(stream),
					     .consumer_position = ringbuffer_position_new(stream)};
	return runtime;
}

size_t ringbuffer_runtime_consume(struct ringbuffer_runtime *runtime, void *data)
{
	/* Ringbuffer if empty*/
	if (ringbuffer_position_equal(&runtime->producer_position, &runtime->consumer_position)) {
		return 0;
	}
	struct pmemstream_entry entry = runtime->consumer_position.current_entry;

	const void *entry_data = pmemstream_entry_data(runtime->stream, entry);
	const size_t entry_length = pmemstream_entry_length(runtime->stream, entry);

	memcpy(data, entry_data, entry_length);

	ringbuffer_position_next(&runtime->consumer_position);

	return entry_length;
}

/* Copy data to ringbuffer */
bool ringbuffer_runtime_produce(struct ringbuffer_runtime *runtime, const void *data, size_t size)
{
	//	struct ringbuffer_position *current_position = &(runtime->producer_position);

	struct pmemstream_entry new_entry;

	int ret = pmemstream_append(runtime->stream, runtime->producer_position.current_region, NULL, data, size,
				    &new_entry);

	//	struct ringbuffer_position next_position = runtime->producer_position;
	if (ringbuffer_position_next(&runtime->producer_position) != 0) {
		int realocate_ret = ringbuffer_runtime_reallocate_region(runtime);
		(void)realocate_ret;
		if (ret != 0) {
			ret = pmemstream_append(runtime->stream, runtime->producer_position.current_region, NULL, data,
						size, &new_entry);
		}
	}
	if (ret != 0) {
		return false;
	}
	//	runtime->producer_position = next_position;

	return true;
}

void ringbuffer_runtime_delete(struct ringbuffer_runtime *runtime)
{
	ringbuffer_position_delete(&runtime->producer_position);
	ringbuffer_position_delete(&runtime->consumer_position);
}
