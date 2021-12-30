// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

#include "region.h"
#include "iterator.h"
#include "libpmemstream_internal.h"

#include <assert.h>
#include <errno.h>

struct region_contexts_map *region_contexts_map_new(void)
{
	struct region_contexts_map *map = calloc(1, sizeof(*map));
	if (!map) {
		return NULL;
	}

	map->container = critnib_new();
	if (!map->container) {
		region_contexts_map_destroy(map);
		return NULL;
	}

	int ret = pthread_mutex_init(&map->lock, NULL);
	if (ret) {
		region_contexts_map_destroy(map);
		return NULL;
	}

	return map;
}

static int free_region_context_cb(uintptr_t key, void *value, void *privdata)
{
	free(value);
	return 0;
}

void region_contexts_map_destroy(struct region_contexts_map *map)
{
	critnib_iter(map->container, 0, (uint64_t)-1, free_region_context_cb, NULL);
	critnib_delete(map->container);

	/* XXX: Handle error */
	pthread_mutex_destroy(&map->lock);

	free(map);
}

int region_contexts_map_get_or_create(struct region_contexts_map *map, struct pmemstream_region region,
				      struct pmemstream_region_context **container_handle)
{
	struct pmemstream_region_context *ctx = critnib_get(map->container, region.offset);
	if (ctx) {
		if (container_handle) {
			*container_handle = ctx;
		}
		return 0;
	}

	int ret = -1;

	pthread_mutex_lock(&map->lock);
	ctx = calloc(1, sizeof(*ctx));
	if (ctx) {
		ret = critnib_insert(map->container, region.offset, ctx, 0 /* no update */);
	}
	pthread_mutex_unlock(&map->lock);

	if (ret) {
		/* Insert failed, free the context. */
		free(ctx);

		if (ret == EEXIST) {
			/* Someone else inserted the region context - just get a pointer to it. */
			ctx = critnib_get(map->container, region.offset);
			assert(ctx);
		} else {
			return ret;
		}
	}

	if (container_handle) {
		*container_handle = ctx;
	}

	return 0;
}

void region_contexts_map_remove(struct region_contexts_map *map, struct pmemstream_region region)
{
	struct pmemstream_region_context *ctx = critnib_remove(map->container, region.offset);
	free(ctx);
}

int region_is_recovered(struct pmemstream_region_context *region_context)
{
	return __atomic_load_n(&region_context->append_offset, __ATOMIC_ACQUIRE) != PMEMSTREAM_OFFSET_UNINITIALIZED;
}

/* Iterates over entire region. Might perform recovery. */
static int region_iterate_and_try_recover(struct pmemstream *stream, struct pmemstream_region region)
{
	struct pmemstream_entry_iterator iter;
	int ret = entry_iterator_initialize(&iter, stream, region);
	if (ret) {
		return ret;
	}

	while (pmemstream_entry_iterator_next(&iter, NULL, NULL) == 0) {
	}

	return 0;
}

int region_try_recover_locked(struct pmemstream *stream, struct pmemstream_region region,
			      struct pmemstream_region_context *region_context)
{
	assert(region_context);
	int ret = 0;

	/* If region is not recovered, iterate over region and perform recovery.
	 * Uses "double-checked locking". */
	if (!region_is_recovered(region_context)) {
		/* XXX: this can be per-region lock */
		pthread_mutex_lock(&stream->region_contexts_map->lock);
		if (!region_is_recovered(region_context)) {
			ret = region_iterate_and_try_recover(stream, region);
		}
		pthread_mutex_unlock(&stream->region_contexts_map->lock);
	}

	return ret;
}

void region_recover(struct pmemstream *stream, struct pmemstream_region region,
		    struct pmemstream_region_context *region_context, struct pmemstream_entry tail)
{
	assert(region_context);
	assert(tail.offset != PMEMSTREAM_OFFSET_UNINITIALIZED);

	struct span_runtime region_rt = span_get_region_runtime(stream, region.offset);
	size_t region_end_offset = region.offset + region_rt.total_size;
	size_t remaining_size = region_end_offset - tail.offset;

	uint64_t expected_append_offset = PMEMSTREAM_OFFSET_UNINITIALIZED;
	int weak = 0; /* Use compare_exchange int strong variation. */
	if (__atomic_compare_exchange_n(&region_context->append_offset, &expected_append_offset, tail.offset, weak,
					__ATOMIC_RELEASE, __ATOMIC_RELAXED)) {
		span_create_empty(stream, tail.offset, remaining_size - SPAN_EMPTY_METADATA_SIZE);
	} else {
		/* Nothing to do, someone else already performed recovery. */
	}
}
