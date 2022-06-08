// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

/* Implementation of public C API */

#include "common/util.h"
#include "libpmemstream_internal.h"
#include "region.h"
#include "region_allocator/region_allocator.h"

#include <assert.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

static int pmemstream_is_initialized(struct pmemstream *stream)
{
	if (strcmp(stream->header->signature, PMEMSTREAM_SIGNATURE) != 0) {
		return -1;
	}
	if (stream->header->block_size != stream->block_size) {
		return -1; // todo: fail with incorrect args or something
	}
	if (stream->header->stream_size != stream->stream_size) {
		return -1; // todo: fail with incorrect args or something
	}

	return 0;
}

static void pmemstream_init(struct pmemstream *stream)
{
	stream->data.memset(stream->header->signature, 0, PMEMSTREAM_SIGNATURE_SIZE,
			    PMEM2_F_MEM_NONTEMPORAL | PMEM2_F_MEM_NODRAIN);

	allocator_initialize(&stream->data, &stream->header->region_allocator_header, stream->usable_size);

	stream->header->stream_size = stream->stream_size;
	stream->header->block_size = stream->block_size;
	stream->header->persisted_timestamp = PMEMSTREAM_INVALID_TIMESTAMP;
	stream->data.persist(stream->header, sizeof(struct pmemstream_header));

	stream->data.memcpy(stream->header->signature, PMEMSTREAM_SIGNATURE, strlen(PMEMSTREAM_SIGNATURE),
			    PMEM2_F_MEM_NONTEMPORAL);
}

static size_t pmemstream_header_size_aligned(size_t block_size)
{
	return ALIGN_UP(sizeof(struct pmemstream_header), block_size);
}

static size_t pmemstream_usable_size(size_t stream_size, size_t block_size)
{
	assert(stream_size >= pmemstream_header_size_aligned(block_size));
	assert(stream_size - pmemstream_header_size_aligned(block_size) >= block_size);
	return ALIGN_DOWN(stream_size - pmemstream_header_size_aligned(block_size), block_size);
}

static int pmemstream_validate_sizes(size_t block_size, struct pmem2_map *map)
{
	if (block_size == 0) {
		return -1;
	}

	if (block_size % CACHELINE_SIZE != 0) {
		return -1;
	}

	if (!IS_POW2(block_size)) {
		return -1;
	}

	if (map == NULL) {
		return -1;
	}

	size_t stream_size = pmem2_map_get_size(map);
	if (stream_size > PTRDIFF_MAX) {
		return -1;
	}

	if (stream_size <= pmemstream_header_size_aligned(block_size)) {
		return -1;
	}

	if (pmemstream_usable_size(stream_size, block_size) <= sizeof(struct span_region)) {
		return -1;
	}

	if (pmemstream_usable_size(stream_size, block_size) < block_size) {
		return -1;
	}

	return 0;
}

/* XXX: this function could be made asynchronous perhaps? */
// XXX: test this: crash before committing new entry and then
// on restart, add new entry (should have same timestamp), verify
// that the unfinished entry is not visible.
static int pmemstream_mark_regions_for_recovery(struct pmemstream *stream)
{
	struct pmemstream_region_iterator *iterator;
	int ret = pmemstream_region_iterator_new(&iterator, stream);
	if (ret) {
		return ret;
	}

	/* XXX: we could keep list of active regions in stream header/lanes and only iterate over them. */
	struct pmemstream_region region;
	pmemstream_region_iterator_seek_first(iterator);

	while (pmemstream_region_iterator_is_valid(iterator) == 0) {
		region = pmemstream_region_iterator_get(iterator);
		struct span_region *span_region =
			(struct span_region *)span_offset_to_span_ptr(&stream->data, region.offset);
		if (span_region->max_valid_timestamp == UINT64_MAX) {
			span_region->max_valid_timestamp = stream->header->persisted_timestamp;
			stream->data.flush(&span_region->max_valid_timestamp, sizeof(span_region->max_valid_timestamp));
		} else {
			/* If max_valid_timestamp is equal to a valid timestamp, this means that these regions
			 * hasn't recovered after previous restart yet, skip it. */
		}
		pmemstream_region_iterator_next(iterator);
	}
	stream->data.drain();

	pmemstream_region_iterator_delete(&iterator);

	return 0;
}

static int pmemstream_initialize_async_ops_locks(struct pmemstream *stream)
{
	// XXX: aligned alloc?
	stream->async_ops_locks = malloc(PMEMSTREAM_MAX_CONCURRENCY * sizeof(*stream->async_ops_locks));
	if (!stream->async_ops_locks) {
		return -1;
	}

	for (size_t locks_initialized = 0; locks_initialized < PMEMSTREAM_MAX_CONCURRENCY; locks_initialized++) {
		int ret = pthread_mutex_init(&stream->async_ops_locks[locks_initialized], NULL);
		if (ret) {
			free(stream->async_ops_locks);
			for (size_t i = 0; i < locks_initialized; i++) {
				pthread_mutex_destroy(&stream->async_ops_locks[i]);
			}
			return ret;
		}
	}

	return 0;
}

static void pmemstream_destroy_async_ops_locks(struct pmemstream *stream)
{
	for (size_t i = 0; i < PMEMSTREAM_MAX_CONCURRENCY; i++) {
		pthread_mutex_destroy(&stream->async_ops_locks[i]);
	}
	free(stream->async_ops_locks);
}

static int pmemstream_initialize_async_ops(struct pmemstream *stream)
{
	// XXX: aligned alloc?
	stream->async_ops = malloc(PMEMSTREAM_MAX_CONCURRENCY * sizeof(struct async_operation));
	if (!stream->async_ops) {
		return -1;
	}

	for (size_t i = 0; i < PMEMSTREAM_MAX_CONCURRENCY; i++) {
		FUTURE_INIT_COMPLETE(&stream->async_ops[i].future);
	}

	return 0;
}

int pmemstream_from_map(struct pmemstream **stream, size_t block_size, struct pmem2_map *map)
{
	if (!stream) {
		return -1;
	}

	if (pmemstream_validate_sizes(block_size, map)) {
		return -1;
	}

	struct pmemstream *s = aligned_alloc(alignof(struct pmemstream), sizeof(struct pmemstream));
	if (!s) {
		return -1;
	}

	size_t spans_offset = pmemstream_header_size_aligned(block_size);
	s->header = pmem2_map_get_address(map);
	s->stream_size = pmem2_map_get_size(map);
	s->usable_size = pmemstream_usable_size(s->stream_size, block_size);
	s->block_size = block_size;

	s->data.base = ((uint8_t *)pmem2_map_get_address(map)) + spans_offset;
	s->data.memcpy = pmem2_get_memcpy_fn(map);
	s->data.memset = pmem2_get_memset_fn(map);
	s->data.persist = pmem2_get_persist_fn(map);
	s->data.flush = pmem2_get_flush_fn(map);
	s->data.drain = pmem2_get_drain_fn(map);

	if (pmemstream_is_initialized(s) != 0) {
		pmemstream_init(s);
	}

	s->committed_timestamp = s->header->persisted_timestamp;
	s->next_timestamp = s->header->persisted_timestamp + 1;

	allocator_runtime_initialize(&s->data, &s->header->region_allocator_header);

	int ret = pmemstream_mark_regions_for_recovery(s);
	if (ret) {
		return ret;
	}

	s->region_runtimes_map = region_runtimes_map_new(&s->data);
	if (!s->region_runtimes_map) {
		goto err_region_runtimes;
	}

	ret = pmemstream_initialize_async_ops_locks(s);
	if (ret) {
		goto err_async_ops_locks;
	}

	ret = pmemstream_initialize_async_ops(s);
	if (ret) {
		goto err_async_ops;
	}

	ret = pthread_mutex_init(&s->commit_lock, NULL);
	if (ret) {
		goto err_commit_lock;
	}

	s->data_mover_sync = data_mover_sync_new();
	if (!s->data_mover_sync) {
		goto err_data_mover;
	}

	ret = sem_init(&s->async_ops_semaphore, 0, PMEMSTREAM_MAX_CONCURRENCY);
	if (ret) {
		goto err_sem_init;
	}

	*stream = s;
	return 0;

err_sem_init:
	data_mover_sync_delete(s->data_mover_sync);
err_data_mover:
	pthread_mutex_destroy(&s->commit_lock);
err_commit_lock:
	free(s->async_ops);
err_async_ops:
	pmemstream_destroy_async_ops_locks(s);
err_async_ops_locks:
	region_runtimes_map_destroy(s->region_runtimes_map);
err_region_runtimes:
	free(s);
	return -1;
}

void pmemstream_delete(struct pmemstream **stream)
{
	if (!stream) {
		return;
	}
	struct pmemstream *s = *stream;

	region_runtimes_map_destroy(s->region_runtimes_map);
	pmemstream_destroy_async_ops_locks(s);
	free(s->async_ops);
	pthread_mutex_destroy(&s->commit_lock);
	data_mover_sync_delete(s->data_mover_sync);
	sem_destroy(&s->async_ops_semaphore);

	free(s);
	*stream = NULL;
}

// XXX: add more tests using these 2 timestamp functions
uint64_t pmemstream_persisted_timestamp(struct pmemstream *stream)
{
	/* Make sure that persisted_timestamp is actually persisted before returning. */
	uint64_t timestamp = __atomic_load_n(&stream->header->persisted_timestamp, __ATOMIC_ACQUIRE);
	stream->data.persist(&stream->header->persisted_timestamp, sizeof(uint64_t));
	return timestamp;
}

uint64_t pmemstream_committed_timestamp(struct pmemstream *stream)
{
	return __atomic_load_n(&stream->committed_timestamp, __ATOMIC_ACQUIRE);
}

static size_t pmemstream_region_total_size_aligned(struct pmemstream *stream, size_t size)
{
	struct span_region span_region = {.span_base = span_base_create(size, SPAN_REGION)};
	return ALIGN_UP(span_get_total_size(&span_region.span_base), stream->block_size);
}

// stream owns the region object - the user gets a reference, but it's not
// necessary to hold on to it and explicitly delete it.
int pmemstream_region_allocate(struct pmemstream *stream, size_t size, struct pmemstream_region *region)
{
	// XXX: lock

	if (!stream || !size) {
		return -1;
	}

	size_t total_size = pmemstream_region_total_size_aligned(stream, size);
	size_t requested_size = total_size - sizeof(struct span_region);

	const uint64_t offset =
		allocator_region_allocate(&stream->data, &stream->header->region_allocator_header, requested_size);
	if (offset == PMEMSTREAM_INVALID_OFFSET) {
		return -1;
	}

	if (region) {
		region->offset = offset;
	}

#ifndef NDEBUG
	const struct span_base *span_base = span_offset_to_span_ptr(&stream->data, offset);
	const struct span_region *span_region = (const struct span_region *)span_base;
	assert(offset % stream->block_size == 0);
	assert(span_get_type(span_base) == SPAN_REGION);
	assert(span_get_total_size(span_base) == total_size);
	assert(((uintptr_t)span_region->data) % CACHELINE_SIZE == 0);
#endif

	return 0;
}

size_t pmemstream_region_size(struct pmemstream *stream, struct pmemstream_region region)
{
	int ret = pmemstream_validate_stream_and_offset(stream, region.offset);
	if (ret) {
		return 0;
	}

	const struct span_base *span_region = span_offset_to_span_ptr(&stream->data, region.offset);
	assert(span_get_type(span_region) == SPAN_REGION);
	return span_get_size(span_region);
}

size_t pmemstream_region_usable_size(struct pmemstream *stream, struct pmemstream_region region)
{
	int ret = pmemstream_validate_stream_and_offset(stream, region.offset);
	if (ret) {
		return 0;
	}

	const struct span_base *span_region = span_offset_to_span_ptr(&stream->data, region.offset);
	assert(span_get_type(span_region) == SPAN_REGION);
	uint64_t region_end_offset = region.offset + span_get_total_size(span_region);

	struct pmemstream_region_runtime *region_runtime;
	ret = pmemstream_region_runtime_initialize(stream, region, &region_runtime);
	if (ret) {
		return 0;
	}
	uint64_t append_offset = region_runtime_get_append_offset_relaxed(region_runtime);

	return region_end_offset - append_offset;
}

int pmemstream_region_free(struct pmemstream *stream, struct pmemstream_region region)
{
	// XXX: unlock

	int ret = pmemstream_validate_stream_and_offset(stream, region.offset);
	if (ret) {
		return ret;
	}

	allocator_region_free(&stream->data, &stream->header->region_allocator_header, region.offset);
	region_runtimes_map_remove(stream->region_runtimes_map, region);

	return 0;
}

// returns pointer to the data of the entry
const void *pmemstream_entry_data(struct pmemstream *stream, struct pmemstream_entry entry)
{
	int ret = pmemstream_validate_stream_and_offset(stream, entry.offset);
	if (ret) {
		return NULL;
	}

	struct span_entry *span_entry = (struct span_entry *)span_offset_to_span_ptr(&stream->data, entry.offset);
	return span_entry->data;
}

// returns the size of the entry
size_t pmemstream_entry_length(struct pmemstream *stream, struct pmemstream_entry entry)
{
	int ret = pmemstream_validate_stream_and_offset(stream, entry.offset);
	if (ret) {
		return 0;
	}
	struct span_entry *span_entry = (struct span_entry *)span_offset_to_span_ptr(&stream->data, entry.offset);
	return span_get_size(&span_entry->span_base);
}

uint64_t pmemstream_entry_timestamp(struct pmemstream *stream, struct pmemstream_entry entry)
{
	int ret = pmemstream_validate_stream_and_offset(stream, entry.offset);
	if (ret) {
		return PMEMSTREAM_INVALID_TIMESTAMP;
	}
	struct span_entry *span_entry = (struct span_entry *)span_offset_to_span_ptr(&stream->data, entry.offset);
	return span_entry->timestamp;
}

int pmemstream_region_runtime_initialize(struct pmemstream *stream, struct pmemstream_region region,
					 struct pmemstream_region_runtime **region_runtime)
{
	int ret = pmemstream_validate_stream_and_offset(stream, region.offset);
	if (ret) {
		return ret;
	}

	ret = region_runtimes_map_get_or_create(stream->region_runtimes_map, region, region_runtime);
	if (ret) {
		return ret;
	}

	assert(*region_runtime);

	return region_runtime_iterate_and_initialize_for_write_locked(stream, region, *region_runtime);
}

static size_t pmemstream_entry_total_size_aligned(size_t size)
{
	struct span_entry span_entry = {.span_base = span_base_create(size, SPAN_ENTRY)};
	return span_get_total_size(&span_entry.span_base);
}

struct async_operation *pmemstream_async_operation(struct pmemstream *stream, uint64_t timestamp)
{
	// XXX: require MAX_CONCURRENCY to be power of two and use bit ops
	uint64_t ops_index = timestamp % PMEMSTREAM_MAX_CONCURRENCY;
	return &stream->async_ops[ops_index];
}

pthread_mutex_t *pmemstream_async_lock(struct pmemstream *stream, uint64_t timestamp)
{
	// XXX: require MAX_CONCURRENCY to be power of two and use bit ops
	uint64_t ops_index = timestamp % PMEMSTREAM_MAX_CONCURRENCY;
	return &stream->async_ops_locks[ops_index];
}

static uint64_t pmemstream_acquire_timestamp(struct pmemstream *stream)
{
	while (sem_trywait(&stream->async_ops_semaphore) != 0) {
		struct pmemstream_async_wait_fut future =
			pmemstream_async_wait_committed(stream, pmemstream_committed_timestamp(stream) + 1);
		while (future_poll(FUTURE_AS_RUNNABLE(&future), NULL) != FUTURE_STATE_COMPLETE)
			;
	}

	uint64_t timestamp = __atomic_fetch_add(&stream->next_timestamp, 1, __ATOMIC_RELAXED);
	pthread_mutex_lock(pmemstream_async_lock(stream, timestamp));
	return timestamp;
}

static void pmemstream_publish_timestamp(struct pmemstream *stream, uint64_t timestamp)
{
	pthread_mutex_unlock(pmemstream_async_lock(stream, timestamp));
}

int pmemstream_reserve(struct pmemstream *stream, struct pmemstream_region region,
		       struct pmemstream_region_runtime *region_runtime, size_t size,
		       struct pmemstream_entry *reserved_entry, void **data_addr)
{
	int ret = pmemstream_validate_stream_and_offset(stream, region.offset);
	if (ret) {
		return ret;
	}

	size_t entry_total_size_span_aligned = pmemstream_entry_total_size_aligned(size);
	const struct span_base *span_region = span_offset_to_span_ptr(&stream->data, region.offset);
	assert(span_get_type(span_region) == SPAN_REGION);

	if (!reserved_entry) {
		return -1;
	}

	if (!region_runtime) {
		ret = pmemstream_region_runtime_initialize(stream, region, &region_runtime);
		if (ret) {
			return ret;
		}
	}

	uint64_t offset = region_runtime_get_append_offset_acquire(region_runtime);
	uint8_t *destination = (uint8_t *)pmemstream_offset_to_ptr(&stream->data, offset);
	assert(offset >= region.offset + offsetof(struct span_region, data));
	if (offset + entry_total_size_span_aligned > region.offset + span_get_total_size(span_region)) {
		return -1;
	}

	region_runtime_increase_append_offset(region_runtime, entry_total_size_span_aligned);

	reserved_entry->offset = offset;
	/* data is right after the entry metadata */
	*data_addr = destination + sizeof(struct span_entry);

	return ret;
}

int pmemstream_publish(struct pmemstream *stream, struct pmemstream_region region,
		       struct pmemstream_region_runtime *region_runtime, struct pmemstream_entry entry, size_t size)
{
	int ret = pmemstream_async_publish(stream, region, region_runtime, entry, size);
	if (ret) {
		return ret;
	}

	// XXX: runtime_wait or blocking call
	uint64_t timestamp = pmemstream_entry_timestamp(stream, entry);
	struct pmemstream_async_wait_fut future = pmemstream_async_wait_persisted(stream, timestamp);
	while (future_poll(FUTURE_AS_RUNNABLE(&future), NULL) != FUTURE_STATE_COMPLETE)
		;

	return 0;
}

// synchronously appends data buffer to the end of the region
int pmemstream_append(struct pmemstream *stream, struct pmemstream_region region,
		      struct pmemstream_region_runtime *region_runtime, const void *data, size_t size,
		      struct pmemstream_entry *new_entry)
{
	int ret = pmemstream_validate_stream_and_offset(stream, region.offset);
	if (ret) {
		return ret;
	}

	struct pmemstream_entry entry;
	ret = pmemstream_async_append(stream, data_mover_sync_get_vdm(stream->data_mover_sync), region, region_runtime,
				      data, size, &entry);
	if (ret) {
		return ret;
	}

	if (new_entry) {
		*new_entry = entry;
	}

	// XXX: runtime_wait or blocking call
	struct pmemstream_async_wait_fut future =
		pmemstream_async_wait_persisted(stream, pmemstream_entry_timestamp(stream, entry));
	while (future_poll(FUTURE_AS_RUNNABLE(&future), NULL) != FUTURE_STATE_COMPLETE)
		;

	return 0;
}

static int pmemstream_async_publish_generic(struct pmemstream *stream, struct pmemstream_region region,
					    struct pmemstream_region_runtime *region_runtime,
					    struct vdm_operation_future future, struct pmemstream_entry entry,
					    size_t size)
{
	int ret = pmemstream_validate_stream_and_offset(stream, region.offset);
	if (ret) {
		return ret;
	}

	if (!region_runtime) {
		ret = pmemstream_region_runtime_initialize(stream, region, &region_runtime);
		if (ret) {
			return ret;
		}
	}

	// XXX: can we move it after future_poll?
	uint64_t timestamp = pmemstream_acquire_timestamp(stream);

	struct async_operation *async_op = pmemstream_async_operation(stream, timestamp);
	uint8_t *destination = (uint8_t *)span_offset_to_span_ptr(&stream->data, entry.offset);
	size_t entry_total_size_span_aligned = pmemstream_entry_total_size_aligned(size);

	async_op->future = future;
	async_op->region = region;
	async_op->region_runtime = region_runtime;
	async_op->entry = entry;
	async_op->size = entry_total_size_span_aligned;

	// XXX: once miniasync supports batch operations, we should not call poll here.
	// Instead, we can do it on commit for multiple futures at once, or even create
	// the futures lazily on commit.
	future_poll(FUTURE_AS_RUNNABLE(&async_op->future), NULL);

	/* Clear next entry metadata. */
	struct span_empty span_empty = {.span_base = span_base_create(0, SPAN_EMPTY)};
	span_base_atomic_store((struct span_base *)(destination + entry_total_size_span_aligned), span_empty.span_base);

	/* Store metadata. */
	struct span_entry span_entry = {.span_base = span_base_create(size, SPAN_ENTRY), .timestamp = timestamp};
	span_entry_atomic_store((struct span_entry *)destination, span_entry);

	pmemstream_publish_timestamp(stream, timestamp);

	return 0;
}

int pmemstream_async_publish(struct pmemstream *stream, struct pmemstream_region region,
			     struct pmemstream_region_runtime *region_runtime, struct pmemstream_entry entry,
			     size_t size)
{
	struct vdm_operation_future future = {0};
	FUTURE_INIT_COMPLETE(&future);

	return pmemstream_async_publish_generic(stream, region, region_runtime, future, entry, size);
}

// asynchronously appends data buffer to the end of the region
// To make sure that the entry is actually stored/commited one must call
// pmemstream_async_wait_committed and poll returned future to completion.
int pmemstream_async_append(struct pmemstream *stream, struct vdm *vdm, struct pmemstream_region region,
			    struct pmemstream_region_runtime *region_runtime, const void *data, size_t size,
			    struct pmemstream_entry *new_entry)
{
	if (!region_runtime) {
		int ret = pmemstream_region_runtime_initialize(stream, region, &region_runtime);
		if (ret) {
			return ret;
		}
	}

	struct pmemstream_entry reserved_entry;
	void *reserved_dest;
	int ret = pmemstream_reserve(stream, region, region_runtime, size, &reserved_entry, &reserved_dest);
	if (ret) {
		return ret;
	}

	struct vdm_operation_future future = vdm_memcpy(vdm, reserved_dest, (void *)data, size, 0);
	ret = pmemstream_async_publish_generic(stream, region, region_runtime, future, reserved_entry, size);
	if (ret) {
		return ret;
	}

	if (new_entry) {
		*new_entry = reserved_entry;
	}

	return 0;
}

static enum future_state pmemstream_async_wait_committed_impl(struct future_context *ctx,
							      struct future_notifier *notifier)
{
	/* XXX: properly use/fix notifier. We can probably use the same as the async_op which we are polling. */
	if (notifier != NULL) {
		notifier->notifier_used = FUTURE_NOTIFIER_NONE;
	}

	struct pmemstream_async_wait_data *data = future_context_get_data(ctx);
	struct pmemstream_async_wait_output *out = future_context_get_output(ctx);
	out->error_code = 0;

	uint64_t committed_timestamp = __atomic_load_n(&data->stream->committed_timestamp, __ATOMIC_ACQUIRE);

	if (data->timestamp <= committed_timestamp)
		return FUTURE_STATE_COMPLETE;

	assert(data->timestamp < __atomic_load_n(&data->stream->next_timestamp, __ATOMIC_RELAXED));

	if (pthread_mutex_trylock(&data->stream->commit_lock) != 0) {
		return FUTURE_STATE_RUNNING;
	}

	bool future_complete = false;
	if (committed_timestamp == __atomic_load_n(&data->stream->committed_timestamp, __ATOMIC_RELAXED)) {
		if (pthread_mutex_trylock(pmemstream_async_lock(data->stream, committed_timestamp + 1)) != 0) {
			pthread_mutex_unlock(&data->stream->commit_lock);
			return FUTURE_STATE_RUNNING;
		}

		struct async_operation *async_op = pmemstream_async_operation(data->stream, committed_timestamp + 1);
		if (future_poll(FUTURE_AS_RUNNABLE(&async_op->future), NULL) == FUTURE_STATE_COMPLETE) {
			/* XXX: we can combine multiple persist into one. */
			const uint8_t *destination =
				(const uint8_t *)pmemstream_offset_to_ptr(&data->stream->data, async_op->entry.offset);
			data->stream->data.persist(destination, async_op->size + sizeof(struct span_entry));

			__atomic_fetch_add(&data->stream->committed_timestamp, 1, __ATOMIC_RELEASE);
			future_complete = data->timestamp <= committed_timestamp + 1;

			sem_post(&data->stream->async_ops_semaphore);
		}

		pthread_mutex_unlock(pmemstream_async_lock(data->stream, committed_timestamp + 1));
	}

	pthread_mutex_unlock(&data->stream->commit_lock);

	return future_complete ? FUTURE_STATE_COMPLETE : FUTURE_STATE_RUNNING;
}

static enum future_state pmemstream_async_wait_persisted_impl(struct future_context *ctx,
							      struct future_notifier *notifier)
{
	/* XXX: properly use/fix notifier. We can probably use the same as the async_op which we are polling. */
	if (notifier != NULL) {
		notifier->notifier_used = FUTURE_NOTIFIER_NONE;
	}

	struct pmemstream_async_wait_data *data = future_context_get_data(ctx);
	struct pmemstream_async_wait_output *out = future_context_get_output(ctx);
	out->error_code = 0;

	uint64_t persisted_timestamp = pmemstream_persisted_timestamp(data->stream);
	if (data->timestamp <= persisted_timestamp)
		return FUTURE_STATE_COMPLETE;

	struct pmemstream_async_wait_fut future = pmemstream_async_wait_committed(data->stream, data->timestamp);
	if (future_poll(FUTURE_AS_RUNNABLE(&future), NULL) == FUTURE_STATE_RUNNING) {
		return FUTURE_STATE_RUNNING;
	}

	bool weak = false;
	bool success = __atomic_compare_exchange_n(&data->stream->header->persisted_timestamp, &persisted_timestamp,
						   data->timestamp, weak, __ATOMIC_RELEASE, __ATOMIC_RELAXED);
	if (success) {
		data->stream->data.persist(&data->stream->header->persisted_timestamp, sizeof(uint64_t));
		return FUTURE_STATE_COMPLETE;
	}

	return FUTURE_STATE_RUNNING;
}

struct pmemstream_async_wait_fut pmemstream_async_wait_committed(struct pmemstream *stream, uint64_t timestamp)
{
	struct pmemstream_async_wait_fut future = {.output.error_code = 0};
	future.data.stream = stream;
	future.data.timestamp = timestamp;
	future.data.region_runtime = NULL;
	FUTURE_INIT(&future, pmemstream_async_wait_committed_impl);

	return future;
}

struct pmemstream_async_wait_fut pmemstream_async_wait_persisted(struct pmemstream *stream, uint64_t timestamp)
{
	struct pmemstream_async_wait_fut future = {.output.error_code = 0};
	future.data.stream = stream;
	future.data.timestamp = timestamp;
	future.data.region_runtime = NULL;
	FUTURE_INIT(&future, pmemstream_async_wait_persisted_impl);

	return future;
}
