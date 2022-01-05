// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

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

	int ret = pthread_mutex_init(&map->container_lock, NULL);
	if (ret) {
		goto err_container_lock;
	}

	ret = pthread_mutex_init(&map->region_lock, NULL);
	if (ret) {
		goto err_region_lock;
	}

	map->container = critnib_new();
	if (!map->container) {
		goto err_critnib;
	}

	return map;

err_critnib:
	pthread_mutex_destroy(&map->region_lock);
err_region_lock:
	pthread_mutex_destroy(&map->container_lock);
err_container_lock:
	free(map);
	return NULL;
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
	pthread_mutex_destroy(&map->region_lock);
	pthread_mutex_destroy(&map->container_lock);

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

	pthread_mutex_lock(&map->container_lock);
	ctx = calloc(1, sizeof(*ctx));
	if (ctx) {
		ret = critnib_insert(map->container, region.offset, ctx, 0 /* no update */);
	}
	pthread_mutex_unlock(&map->container_lock);

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

int region_is_append_offset_initialized(struct pmemstream_region_context *region_context)
{
	return __atomic_load_n(&region_context->append_offset, __ATOMIC_ACQUIRE) != PMEMSTREAM_OFFSET_UNINITIALIZED;
}

/* Iterates over entire region. Might initialize append_offset. */
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

int region_try_initialize_append_offset_locked(struct pmemstream *stream, struct pmemstream_region region,
					       struct pmemstream_region_context *region_context)
{
	assert(region_context);
	int ret = 0;

	/* If append_offset is not set, iterate over region and set it after last valid entry.
	 * Uses "double-checked locking". */
	if (!region_is_append_offset_initialized(region_context)) {
		pthread_mutex_lock(&stream->region_contexts_map->region_lock);
		if (!region_is_append_offset_initialized(region_context)) {
			ret = region_iterate_and_try_recover(stream, region);
		}
		pthread_mutex_unlock(&stream->region_contexts_map->region_lock);
	}

	return ret;
}

void region_initialize_append_offset(struct pmemstream *stream, struct pmemstream_region region,
				     struct pmemstream_region_context *region_context, struct pmemstream_entry tail)
{
	assert(region_context);
	assert(tail.offset != PMEMSTREAM_OFFSET_UNINITIALIZED);

	uint64_t expected_append_offset = PMEMSTREAM_OFFSET_UNINITIALIZED;
	uint64_t desired = tail.offset | PMEMSTREAM_OFFSET_DIRTY_BIT;
	int weak = 0; /* Use compare_exchange int strong variation. */
	__atomic_compare_exchange_n(&region_context->append_offset, &expected_append_offset, desired, weak,
				    __ATOMIC_RELEASE, __ATOMIC_RELAXED);
}
