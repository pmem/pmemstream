// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include "region.h"
#include "iterator.h"
#include "libpmemstream_internal.h"

#include <assert.h>
#include <errno.h>

struct region_runtimes_map *region_runtimes_map_new(void)
{
	struct region_runtimes_map *map = calloc(1, sizeof(*map));
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

static int free_region_runtime_cb(uintptr_t key, void *value, void *privdata)
{
	free(value);
	return 0;
}

void region_runtimes_map_destroy(struct region_runtimes_map *map)
{
	critnib_iter(map->container, 0, (uint64_t)-1, free_region_runtime_cb, NULL);
	critnib_delete(map->container);

	/* XXX: Handle error */
	pthread_mutex_destroy(&map->region_lock);
	pthread_mutex_destroy(&map->container_lock);

	free(map);
}

int region_runtimes_map_get_or_create(struct region_runtimes_map *map, struct pmemstream_region region,
				      struct pmemstream_region_runtime **container_handle)
{
	struct pmemstream_region_runtime *runtime = critnib_get(map->container, region.offset);
	if (runtime) {
		if (container_handle) {
			*container_handle = runtime;
		}
		return 0;
	}

	int ret = -1;

	pthread_mutex_lock(&map->container_lock);
	runtime = calloc(1, sizeof(*runtime));
	if (runtime) {
		ret = critnib_insert(map->container, region.offset, runtime, 0 /* no update */);
	}
	pthread_mutex_unlock(&map->container_lock);

	if (ret) {
		/* Insert failed, free the runtime. */
		free(runtime);

		if (ret == EEXIST) {
			/* Someone else inserted the region runtime - just get a pointer to it. */
			runtime = critnib_get(map->container, region.offset);
			assert(runtime);
		} else {
			return ret;
		}
	}

	if (container_handle) {
		*container_handle = runtime;
	}

	return 0;
}

void region_runtimes_map_remove(struct region_runtimes_map *map, struct pmemstream_region region)
{
	struct pmemstream_region_runtime *runtime = critnib_remove(map->container, region.offset);
	free(runtime);
}

int region_is_runtime_initialized(const struct pmemstream_region_runtime *region_runtime)
{
	return __atomic_load_n(&region_runtime->append_offset, __ATOMIC_ACQUIRE) != PMEMSTREAM_OFFSET_UNINITIALIZED;
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

int region_try_runtime_initialize_locked(struct pmemstream *stream, struct pmemstream_region region,
					 struct pmemstream_region_runtime *region_runtime)
{
	assert(region_runtime);
	int ret = 0;

	/* If append_offset is not set, iterate over region and set it after last valid entry.
	 * Uses "double-checked locking". */
	if (!region_is_runtime_initialized(region_runtime)) {
		pthread_mutex_lock(&stream->region_runtimes_map->region_lock);
		if (!region_is_runtime_initialized(region_runtime)) {
			ret = region_iterate_and_try_recover(stream, region);
		}
		pthread_mutex_unlock(&stream->region_runtimes_map->region_lock);
	}

	return ret;
}

void region_runtime_initialize(struct pmemstream_region_runtime *region_runtime, struct pmemstream_entry tail)
{
	assert(region_runtime);
	assert(tail.offset != PMEMSTREAM_OFFSET_UNINITIALIZED);

	uint64_t expected_append_offset = PMEMSTREAM_OFFSET_UNINITIALIZED;
	uint64_t desired = tail.offset | PMEMSTREAM_OFFSET_DIRTY_BIT;
	int weak = 0; /* Use compare_exchange int strong variation. */
	__atomic_compare_exchange_n(&region_runtime->append_offset, &expected_append_offset, desired, weak,
				    __ATOMIC_RELEASE, __ATOMIC_RELAXED);
}
