// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include "region.h"
#include "iterator.h"
#include "libpmemstream_internal.h"

#include <assert.h>
#include <errno.h>

#define PMEMSTREAM_OFFSET_UNINITIALIZED 0ULL
#define PMEMSTREAM_OFFSET_DIRTY_BIT (1ULL << 63)
#define PMEMSTREAM_OFFSET_DIRTY_MASK (~PMEMSTREAM_OFFSET_DIRTY_BIT)

/*
 * It contains all runtime data specific to a region.
 * It is always managed by the pmemstream (user can only obtain a non-owning pointer) and can be created
 * in few different ways:
 * - By explicitly calling pmemstream_get_region_runtime() for the first time
 * - By calling pmemstream_append (only if region_runtime does not exist yet)
 * - By advancing an entry iterator past last entry in a region (only if region_runtime does not exist yet)
 */
struct pmemstream_region_runtime {
	/*
	 * Offset at which new entries will be appended. Also serves as indicator of region runtime initialization
	 * state.
	 *
	 * Can be int 3 different states:
	 * - uninitialized (append_offset == PMEMSTREAM_OFFSET_UNINITIALIZED): we don't know what is the proper offset
	 * - dirty (append_offset & PMEMSTREAM_OFFSET_DIRTY_BIT != 0): proper offset is already known but region was not
	 *   cleared yet
	 * - clear (append_offset & PMEMSTREAM_OFFSET_DIRTY_BIT == 0): proper offset is already known and region was
	 *   cleared
	 *
	 * If PMEMSTREAM_OFFSET_DIRTY_BIT is set, append_offset points to a valid location but the memory from
	 * 'append_offset & PMEMSTREAM_OFFSET_DIRTY_MASK' to the end of region was not yet cleared. */
	uint64_t append_offset;

	/*
	 * All entries which start at offset < committed_offset can be treated as committed and safely read
	 * from multiple threads.
	 */
	uint64_t committed_offset;
};

/*
 * Holds mapping between region offset and region_runtime.
 */
struct region_runtimes_map {
	critnib *container;
	pthread_mutex_t region_lock; /* XXX: for multiple regions, we might want to consider having more locks. */
};

struct region_runtimes_map *region_runtimes_map_new(void)
{
	struct region_runtimes_map *map = calloc(1, sizeof(*map));
	if (!map) {
		return NULL;
	}

	int ret = pthread_mutex_init(&map->region_lock, NULL);
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

	free(map);
}

static int region_runtimes_map_create_or_fail(struct region_runtimes_map *map, struct pmemstream_region region,
					      struct pmemstream_region_runtime **container_handle)
{
	assert(container_handle);

	struct pmemstream_region_runtime *runtime = (struct pmemstream_region_runtime *)calloc(1, sizeof(*runtime));
	if (!runtime) {
		return -1;
	}

	int ret = critnib_insert(map->container, region.offset, runtime, 0 /* no update */);
	if (ret) {
		free(runtime);
		return ret;
	}

	*container_handle = runtime;
	return ret;
}

static int region_runtimes_map_create(struct region_runtimes_map *map, struct pmemstream_region region,
				      struct pmemstream_region_runtime **container_handle)
{
	assert(container_handle);
	int ret = region_runtimes_map_create_or_fail(map, region, container_handle);
	if (ret == EEXIST) {
		/* Someone else inserted the region runtime - just get a pointer to it. */
		*container_handle = critnib_get(map->container, region.offset);
		assert(*container_handle);

		return 0;
	}

	return ret;
}

int region_runtimes_map_get_or_create(struct region_runtimes_map *map, struct pmemstream_region region,
				      struct pmemstream_region_runtime **container_handle)
{
	assert(container_handle);

	struct pmemstream_region_runtime *runtime = critnib_get(map->container, region.offset);
	if (runtime) {
		*container_handle = runtime;
		return 0;
	}

	return region_runtimes_map_create(map, region, container_handle);
}

uint64_t region_runtime_get_append_offset_acquire(const struct pmemstream_region_runtime *region_runtime)
{
	assert(region_runtime_is_initialized(region_runtime));
	return __atomic_load_n(&region_runtime->append_offset, __ATOMIC_ACQUIRE) & PMEMSTREAM_OFFSET_DIRTY_MASK;
}

uint64_t region_runtime_get_committed_offset_acquire(const struct pmemstream_region_runtime *region_runtime)
{
	assert(region_runtime_is_initialized(region_runtime));
	return __atomic_load_n(&region_runtime->committed_offset, __ATOMIC_ACQUIRE);
}

void region_runtimes_map_remove(struct region_runtimes_map *map, struct pmemstream_region region)
{
	struct pmemstream_region_runtime *runtime = critnib_remove(map->container, region.offset);
	free(runtime);
}

bool region_runtime_is_initialized(const struct pmemstream_region_runtime *region_runtime)
{
	return __atomic_load_n(&region_runtime->append_offset, __ATOMIC_ACQUIRE) != PMEMSTREAM_OFFSET_UNINITIALIZED;
}

bool region_runtime_is_dirty(const struct pmemstream_region_runtime *region_runtime)
{
	return (__atomic_load_n(&region_runtime->append_offset, __ATOMIC_ACQUIRE) & PMEMSTREAM_OFFSET_DIRTY_BIT) != 0;
}

void region_runtime_increase_append_offset(struct pmemstream_region_runtime *region_runtime, uint64_t diff)
{
	__atomic_fetch_add(&region_runtime->append_offset, diff, __ATOMIC_RELAXED);
}

void region_runtime_increase_committed_offset(struct pmemstream_region_runtime *region_runtime, uint64_t diff)
{
	__atomic_fetch_add(&region_runtime->committed_offset, diff, __ATOMIC_RELEASE);
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

int region_runtime_try_initialize_locked(struct pmemstream *stream, struct pmemstream_region region,
					 struct pmemstream_region_runtime *region_runtime)
{
	assert(region_runtime);
	int ret = 0;

	/* If append_offset is not set, iterate over region and set it after last valid entry.
	 * Uses "double-checked locking". */
	if (!region_runtime_is_initialized(region_runtime)) {
		pthread_mutex_lock(&stream->region_runtimes_map->region_lock);
		if (!region_runtime_is_initialized(region_runtime)) {
			ret = region_iterate_and_try_recover(stream, region);
		}
		pthread_mutex_unlock(&stream->region_runtimes_map->region_lock);
	}

	return ret;
}

/* Initializes append_offset to DIRTY(desired_offset) */
static bool region_runtime_try_initialize_append_offset_relaxed(struct pmemstream_region_runtime *region_runtime,
								uint64_t desired_offset)
{
	uint64_t expected_append_offset = PMEMSTREAM_OFFSET_UNINITIALIZED;
	uint64_t desired = desired_offset | PMEMSTREAM_OFFSET_DIRTY_BIT;
	int weak = 0; /* Use compare_exchange strong variation. */
	return __atomic_compare_exchange_n(&region_runtime->append_offset, &expected_append_offset, desired, weak,
					   __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}

void region_runtime_initialize(struct pmemstream_region_runtime *region_runtime, struct pmemstream_entry tail)
{
	assert(region_runtime);
	assert(tail.offset != PMEMSTREAM_OFFSET_UNINITIALIZED);

	/* We use relaxed here because of release fence at the end. */
	if (region_runtime_try_initialize_append_offset_relaxed(region_runtime, tail.offset)) {
		__atomic_store_n(&region_runtime->committed_offset, tail.offset, __ATOMIC_RELAXED);
	}

	__atomic_thread_fence(__ATOMIC_RELEASE);
}

static void region_runtime_clear_from_tail(struct pmemstream *stream, struct pmemstream_region region,
					   struct pmemstream_region_runtime *region_runtime, uint64_t tail)
{
	struct span_runtime region_rt = span_get_region_runtime(stream, region.offset);
	size_t region_end_offset = region.offset + region_rt.total_size;
	size_t remaining_size = region_end_offset - tail;

	if (!util_is_zeroed(pmemstream_offset_to_ptr(stream, tail), remaining_size)) {
		span_create_empty(stream, tail, remaining_size - SPAN_EMPTY_METADATA_SIZE);
	}

	__atomic_store_n(&region_runtime->append_offset, tail, __ATOMIC_RELEASE);
}

uint64_t region_runtime_try_clear_from_tail(struct pmemstream *stream, struct pmemstream_region region,
					    struct pmemstream_region_runtime *region_runtime)
{
	assert(region_runtime_is_initialized(region_runtime));

	uint64_t tail = __atomic_load_n(&region_runtime->append_offset, __ATOMIC_RELAXED);
	if ((tail & PMEMSTREAM_OFFSET_DIRTY_BIT) != 0) {
		tail &= PMEMSTREAM_OFFSET_DIRTY_MASK;
		region_runtime_clear_from_tail(stream, region, region_runtime, tail);
	}

	assert(!region_runtime_is_dirty(region_runtime));

	return tail;
}
