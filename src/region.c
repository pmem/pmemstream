// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include "region.h"
#include "iterator.h"
#include "libpmemstream_internal.h"

#include <assert.h>
#include <errno.h>

#ifndef NDEBUG
#define PMEMSTREAM_OFFSET_UNINITIALIZED 0ULL
#endif

/*
 * It contains all runtime data specific to a region.
 * It is always managed by the pmemstream (user can only obtain a non-owning pointer) and can be created
 * in few different ways:
 * - By explicitly calling pmemstream_region_runtime_initialize() for the first time
 * - By calling pmemstream_append (only if region_runtime does not exist yet)
 * - By advancing an entry iterator past last entry in a region (only if region_runtime does not exist yet)
 */
struct pmemstream_region_runtime {
	/* State of the region_runtime. */
	enum region_runtime_state state;

	/*
	 * Offset at which new entries will be appended. Also serves as indicator of region runtime initialization
	 * state.
	 */
	uint64_t append_offset;

	/*
	 * All entries which start at offset < committed_offset can be treated as committed and safely read
	 * from multiple threads.
	 */
	uint64_t committed_offset;

	/* Protects region initialization step. */
	pthread_mutex_t region_lock;
};

/*
 * Holds mapping between region offset and region_runtime.
 */
struct region_runtimes_map {
	critnib *container;
};

struct region_runtimes_map *region_runtimes_map_new(void)
{
	struct region_runtimes_map *map = calloc(1, sizeof(*map));
	if (!map) {
		return NULL;
	}

	map->container = critnib_new();
	if (!map->container) {
		goto err_critnib;
	}

	return map;

err_critnib:
	free(map);
	return NULL;
}

static int free_region_runtime_cb(uintptr_t key, void *value, void *privdata)
{
	struct pmemstream_region_runtime *region_runtime = (struct pmemstream_region_runtime *)value;
	assert(region_runtime);

	/* XXX: Handle error */
	pthread_mutex_destroy(&region_runtime->region_lock);

	free(value);
	return 0;
}

void region_runtimes_map_destroy(struct region_runtimes_map *map)
{
	critnib_iter(map->container, 0, UINT64_MAX, free_region_runtime_cb, NULL);
	critnib_delete(map->container);
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

	int ret = pthread_mutex_init(&runtime->region_lock, NULL);
	if (ret) {
		goto err_region_lock;
	}

	ret = critnib_insert(map->container, region.offset, runtime, 0 /* no update */);
	if (ret) {
		goto err_critnib_insert;
	}

	*container_handle = runtime;
	return ret;

err_critnib_insert:
	/* XXX: Handle error */
	pthread_mutex_destroy(&runtime->region_lock);
err_region_lock:
	free(runtime);
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
	assert(region_runtime_get_state_acquire(region_runtime) != REGION_RUNTIME_STATE_UNINITIALIZED);
	return __atomic_load_n(&region_runtime->append_offset, __ATOMIC_ACQUIRE);
}

uint64_t region_runtime_get_committed_offset_acquire(const struct pmemstream_region_runtime *region_runtime)
{
	assert(region_runtime_get_state_acquire(region_runtime) != REGION_RUNTIME_STATE_UNINITIALIZED);
	return __atomic_load_n(&region_runtime->committed_offset, __ATOMIC_ACQUIRE);
}

void region_runtimes_map_remove(struct region_runtimes_map *map, struct pmemstream_region region)
{
	struct pmemstream_region_runtime *runtime = critnib_remove(map->container, region.offset);
	free(runtime);
}

enum region_runtime_state region_runtime_get_state_acquire(const struct pmemstream_region_runtime *region_runtime)
{
	return __atomic_load_n(&region_runtime->state, __ATOMIC_ACQUIRE);
}

void region_runtime_increase_append_offset(struct pmemstream_region_runtime *region_runtime, uint64_t diff)
{
	assert(region_runtime_get_state_acquire(region_runtime) == REGION_RUNTIME_STATE_WRITE_READY);
	__atomic_fetch_add(&region_runtime->append_offset, diff, __ATOMIC_RELAXED);
}

void region_runtime_increase_committed_offset(struct pmemstream_region_runtime *region_runtime, uint64_t diff)
{
	assert(region_runtime_get_state_acquire(region_runtime) == REGION_RUNTIME_STATE_WRITE_READY);
	__atomic_fetch_add(&region_runtime->committed_offset, diff, __ATOMIC_RELEASE);
}

static void region_runtime_initialize_for_read(struct pmemstream_region_runtime *region_runtime,
					       struct pmemstream_entry tail)
{
	/* invariant, region_initialization should always happen under a lock. */
	assert(pthread_mutex_trylock(&region_runtime->region_lock) != 0);
	assert(region_runtime);
	assert(tail.offset != PMEMSTREAM_OFFSET_UNINITIALIZED);

	region_runtime->committed_offset = tail.offset;
	region_runtime->append_offset = tail.offset;
	__atomic_store_n(&region_runtime->state, REGION_RUNTIME_STATE_READ_READY, __ATOMIC_RELEASE);
}

void region_runtime_initialize_for_read_locked(struct pmemstream_region_runtime *region_runtime,
					       struct pmemstream_entry tail)
{
	if (region_runtime_get_state_acquire(region_runtime) == REGION_RUNTIME_STATE_UNINITIALIZED) {
		pthread_mutex_lock(&region_runtime->region_lock);
		if (region_runtime_get_state_acquire(region_runtime) == REGION_RUNTIME_STATE_UNINITIALIZED) {
			region_runtime_initialize_for_read(region_runtime, tail);
		}
		pthread_mutex_unlock(&region_runtime->region_lock);
	}

	/* Now, region_runtime can be 'read_ready' or 'write_ready'. */
	assert(region_runtime_get_state_acquire(region_runtime) != REGION_RUNTIME_STATE_UNINITIALIZED);
}

/* Iterates over entire region. Might initialize region. Should be called under a lock. */
static int region_iterate_and_initialize_for_read(struct pmemstream *stream, struct pmemstream_region region,
						  struct pmemstream_region_runtime *region_runtime)
{
	/* invariant, region_initialization should always happen under a lock. */
	assert(pthread_mutex_trylock(&region_runtime->region_lock) != 0);

	struct pmemstream_entry_iterator iterator;

	/* do not use region_runtime_initialize_for_read_locked - current function is already executing under a
	 * region_lock. */
	int ret = entry_iterator_initialize(&iterator, stream, region, &region_runtime_initialize_for_read);
	if (ret) {
		return ret;
	}

	/* Loop over all entries - initialization will happen when we encounter the last one. */
	while (pmemstream_entry_iterator_next(&iterator) == 0) {
	}

	return 0;
}

static void region_runtime_clear_from_tail(struct pmemstream *stream, struct pmemstream_region region,
					   struct pmemstream_region_runtime *region_runtime)
{
	/* invariant, region_initialization should always happend under a lock. */
	assert(pthread_mutex_trylock(&region_runtime->region_lock) != 0);
	assert(region_runtime_get_state_acquire(region_runtime) == REGION_RUNTIME_STATE_READ_READY);

	uint64_t append_offset = region_runtime_get_append_offset_acquire(region_runtime);
	const struct span_base *span_base = span_offset_to_span_ptr(&stream->data, region.offset);
	size_t region_end_offset = region.offset + span_get_total_size(span_base);
	size_t remaining_size = region_end_offset - append_offset;

	if (remaining_size != 0) {
		struct span_empty span_empty = {
			.span_base = span_base_create(remaining_size - sizeof(span_empty), SPAN_EMPTY)};

		uint8_t *destination = (uint8_t *)span_offset_to_span_ptr(&stream->data, append_offset);
		stream->data.memcpy(destination, &span_empty, sizeof(span_empty), PMEM2_F_MEM_NOFLUSH);
		stream->data.memset(destination + sizeof(span_empty), 0, remaining_size - sizeof(span_empty),
				    PMEM2_F_MEM_NONTEMPORAL | PMEM2_F_MEM_NODRAIN);
		stream->data.persist(destination, sizeof(span_empty));
	}

	__atomic_store_n(&region_runtime->state, REGION_RUNTIME_STATE_WRITE_READY, __ATOMIC_RELEASE);

	assert(region_runtime_get_state_acquire(region_runtime) == REGION_RUNTIME_STATE_WRITE_READY);
}

int region_runtime_initialize_for_write_locked(struct pmemstream *stream, struct pmemstream_region region,
					       struct pmemstream_region_runtime *region_runtime)
{
	int ret = 0;
	assert(region_runtime);

	if (region_runtime_get_state_acquire(region_runtime) == REGION_RUNTIME_STATE_WRITE_READY) {
		/* Nothing to do . */
		return 0;
	}

	pthread_mutex_lock(&region_runtime->region_lock);

	enum region_runtime_state state_locked = region_runtime_get_state_acquire(region_runtime);
	if (state_locked == REGION_RUNTIME_STATE_UNINITIALIZED) {
		ret = region_iterate_and_initialize_for_read(stream, region, region_runtime);
		region_runtime_clear_from_tail(stream, region, region_runtime);
	} else if (state_locked == REGION_RUNTIME_STATE_READ_READY) {
		region_runtime_clear_from_tail(stream, region, region_runtime);
	} else {
		/* Write_ready. Nothing to do. */
	}

	pthread_mutex_unlock(&region_runtime->region_lock);

	assert(region_runtime_get_state_acquire(region_runtime) == REGION_RUNTIME_STATE_WRITE_READY);
	assert(region_runtime_get_append_offset_acquire(region_runtime) != PMEMSTREAM_OFFSET_UNINITIALIZED);
	return ret;
}
