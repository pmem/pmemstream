// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

/* Implementation of public C API */

#include "common/util.h"
#include "libpmemstream_internal.h"

#include <assert.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *pmemstream_offset_to_ptr(struct pmemstream *stream, size_t offset)
{
	return (uint8_t *)stream->data->spans + offset;
}

static pmemstream_span_bytes *pmemstream_offset_to_span_ptr(struct pmemstream *stream, size_t offset)
{
	assert(offset % sizeof(pmemstream_span_bytes) == 0);

	return (pmemstream_span_bytes *)pmemstream_offset_to_ptr(stream, offset);
}

static void pmemstream_span_create_empty(struct pmemstream *stream, uint64_t offset, size_t data_size)
{
	pmemstream_span_bytes *span = pmemstream_offset_to_span_ptr(stream, offset);
	assert((data_size & PMEMSTREAM_SPAN_TYPE_MASK) == 0);
	span[0] = data_size | PMEMSTREAM_SPAN_EMPTY;

	void *dest = ((uint8_t *)span) + SPAN_EMPTY_METADATA_SIZE;
	stream->memset(dest, 0, data_size, PMEM2_F_MEM_NONTEMPORAL | PMEM2_F_MEM_NODRAIN);
	stream->persist(span, SPAN_EMPTY_METADATA_SIZE);
}

static void pmemstream_span_create_entry(struct pmemstream *stream, uint64_t offset, const void *data, size_t data_size,
					 size_t popcount)
{
	pmemstream_span_bytes *span = pmemstream_offset_to_span_ptr(stream, offset);
	assert((data_size & PMEMSTREAM_SPAN_TYPE_MASK) == 0);
	span[0] = data_size | PMEMSTREAM_SPAN_ENTRY;
	span[1] = popcount;

	// XXX - use variadic mempcy to store data and metadata at once
	void *dest = ((uint8_t *)span) + SPAN_ENTRY_METADATA_SIZE;
	stream->memcpy(dest, data, data_size, PMEM2_F_MEM_NONTEMPORAL | PMEM2_F_MEM_NODRAIN);
	stream->persist(span, SPAN_ENTRY_METADATA_SIZE);
}

static void pmemstream_span_create_region(struct pmemstream *stream, uint64_t offset, size_t size)
{
	pmemstream_span_bytes *span = pmemstream_offset_to_span_ptr(stream, offset);
	assert((size & PMEMSTREAM_SPAN_TYPE_MASK) == 0);
	span[0] = size | PMEMSTREAM_SPAN_REGION;

	stream->persist(span, SPAN_REGION_METADATA_SIZE);
}

static uint64_t pmemstream_get_span_size(pmemstream_span_bytes *span)
{
	return span[0] & PMEMSTREAM_SPAN_EXTRA_MASK;
}

static enum pmemstream_span_type pmemstream_get_span_type(pmemstream_span_bytes *span)
{
	return span[0] & PMEMSTREAM_SPAN_TYPE_MASK;
}

static struct pmemstream_span_runtime pmemstream_span_get_empty_runtime(struct pmemstream *stream, uint64_t offset)
{
	pmemstream_span_bytes *span = pmemstream_offset_to_span_ptr(stream, offset);
	struct pmemstream_span_runtime srt;

	assert(pmemstream_get_span_type(span) == PMEMSTREAM_SPAN_EMPTY);

	srt.type = PMEMSTREAM_SPAN_EMPTY;
	srt.empty.size = pmemstream_get_span_size(span);
	srt.data_offset = offset + SPAN_EMPTY_METADATA_SIZE;
	srt.total_size = ALIGN_UP(srt.empty.size + SPAN_EMPTY_METADATA_SIZE, sizeof(pmemstream_span_bytes));

	return srt;
}

static struct pmemstream_span_runtime pmemstream_span_get_entry_runtime(struct pmemstream *stream, uint64_t offset)
{
	pmemstream_span_bytes *span = pmemstream_offset_to_span_ptr(stream, offset);
	struct pmemstream_span_runtime srt;

	assert(pmemstream_get_span_type(span) == PMEMSTREAM_SPAN_ENTRY);

	srt.type = PMEMSTREAM_SPAN_ENTRY;
	srt.entry.size = pmemstream_get_span_size(span);
	srt.entry.popcount = span[1];
	srt.data_offset = offset + SPAN_ENTRY_METADATA_SIZE;
	srt.total_size = ALIGN_UP(srt.entry.size + SPAN_ENTRY_METADATA_SIZE, sizeof(pmemstream_span_bytes));

	return srt;
}

static struct pmemstream_span_runtime pmemstream_span_get_region_runtime(struct pmemstream *stream, uint64_t offset)
{
	pmemstream_span_bytes *span = pmemstream_offset_to_span_ptr(stream, offset);
	struct pmemstream_span_runtime srt;

	assert(pmemstream_get_span_type(span) == PMEMSTREAM_SPAN_REGION);

	srt.type = PMEMSTREAM_SPAN_REGION;
	srt.region.size = pmemstream_get_span_size(span);
	srt.data_offset = offset + SPAN_REGION_METADATA_SIZE;
	srt.total_size = ALIGN_UP(srt.region.size + SPAN_REGION_METADATA_SIZE, sizeof(pmemstream_span_bytes));

	return srt;
}

static struct pmemstream_span_runtime pmemstream_span_get_runtime(struct pmemstream *stream, uint64_t offset)
{
	pmemstream_span_bytes *span = pmemstream_offset_to_span_ptr(stream, offset);
	struct pmemstream_span_runtime srt;

	switch (pmemstream_get_span_type(span)) {
		case PMEMSTREAM_SPAN_EMPTY:
			srt = pmemstream_span_get_empty_runtime(stream, offset);
			break;
		case PMEMSTREAM_SPAN_ENTRY:
			srt = pmemstream_span_get_entry_runtime(stream, offset);
			break;
		case PMEMSTREAM_SPAN_REGION:
			srt = pmemstream_span_get_region_runtime(stream, offset);
			break;
		default:
			abort();
	}

	return srt;
}

static int pmemstream_is_initialized(struct pmemstream *stream)
{
	if (strcmp(stream->data->header.signature, PMEMSTREAM_SIGNATURE) != 0) {
		return -1;
	}
	if (stream->data->header.block_size != stream->block_size) {
		return -1; // todo: fail with incorrect args or something
	}
	if (stream->data->header.stream_size != stream->stream_size) {
		return -1; // todo: fail with incorrect args or something
	}

	return 0;
}

static void pmemstream_init(struct pmemstream *stream)
{
	stream->memset(stream->data->header.signature, 0, PMEMSTREAM_SIGNATURE_SIZE,
		       PMEM2_F_MEM_NONTEMPORAL | PMEM2_F_MEM_NODRAIN);
	stream->data->header.stream_size = stream->stream_size;
	stream->data->header.block_size = stream->block_size;
	stream->persist(stream->data, sizeof(struct pmemstream_data));

	pmemstream_span_create_empty(stream, 0, stream->usable_size - SPAN_EMPTY_METADATA_SIZE);
	stream->memcpy(stream->data->header.signature, PMEMSTREAM_SIGNATURE, strlen(PMEMSTREAM_SIGNATURE),
		       PMEM2_F_MEM_NONTEMPORAL);
}

static int validate_entry(struct pmemstream *stream, struct pmemstream_entry entry)
{
	struct pmemstream_span_runtime srt = pmemstream_span_get_runtime(stream, entry.offset);
	void *entry_data = pmemstream_offset_to_ptr(stream, srt.data_offset);
	if (srt.type == PMEMSTREAM_SPAN_ENTRY &&
	    util_popcount_memory(entry_data, srt.entry.size) == srt.entry.popcount) {
		return 0;
	}
	return -1;
}

int pmemstream_from_map(struct pmemstream **stream, size_t block_size, struct pmem2_map *map)
{
	struct pmemstream *s = malloc(sizeof(struct pmemstream));
	s->data = pmem2_map_get_address(map);
	s->stream_size = pmem2_map_get_size(map);
	s->usable_size = ALIGN_DOWN(s->stream_size - sizeof(struct pmemstream_data), block_size);
	s->block_size = block_size;

	s->memcpy = pmem2_get_memcpy_fn(map);
	s->memset = pmem2_get_memset_fn(map);
	s->persist = pmem2_get_persist_fn(map);
	s->flush = pmem2_get_flush_fn(map);
	s->drain = pmem2_get_drain_fn(map);

	if (pmemstream_is_initialized(s) != 0) {
		pmemstream_init(s);
	}

	s->region_contexts_container = critnib_new();
	if (!s->region_contexts_container) {
		return -1;
	}

	int ret = pthread_mutex_init(&s->region_contexts_container_lock, NULL);
	if (ret) {
		return ret;
	}

	*stream = s;

	return 0;
}

int critnib_free_context(uintptr_t key, void *value, void *privdata)
{
	free(value);
	return 0;
}

void pmemstream_delete(struct pmemstream **stream)
{
	struct pmemstream *s = *stream;

	critnib_iter(s->region_contexts_container, 0, (uint64_t)-1, critnib_free_context, NULL);
	critnib_delete(s->region_contexts_container);

	/* XXX: Handle error */
	pthread_mutex_destroy(&s->region_contexts_container_lock);

	free(s);
	*stream = NULL;
}

// stream owns the region object - the user gets a reference, but it's not
// necessary to hold on to it and explicitly delete it.
int pmemstream_region_allocate(struct pmemstream *stream, size_t size, struct pmemstream_region *region)
{
	const uint64_t offset = 0;
	struct pmemstream_span_runtime srt = pmemstream_span_get_runtime(stream, offset);

	if (srt.type != PMEMSTREAM_SPAN_EMPTY)
		return -1;

	size = ALIGN_UP(size, stream->block_size);

	pmemstream_span_create_region(stream, offset, size);
	region->offset = offset;

	return 0;
}

size_t pmemstream_region_size(struct pmemstream *stream, struct pmemstream_region region)
{
	struct pmemstream_span_runtime region_srt = pmemstream_span_get_region_runtime(stream, region.offset);

	return region_srt.region.size;
}

int pmemstream_region_free(struct pmemstream *stream, struct pmemstream_region region)
{
	struct pmemstream_span_runtime srt = pmemstream_span_get_runtime(stream, region.offset);

	if (srt.type != PMEMSTREAM_SPAN_REGION)
		return -1;

	pmemstream_span_create_empty(stream, 0, stream->usable_size - SPAN_EMPTY_METADATA_SIZE);

	struct pmemstream_region_context *ctx = critnib_remove(stream->region_contexts_container, region.offset);
	free(ctx);

	return 0;
}

// synchronously appends data buffer to the end of the region
int pmemstream_append(struct pmemstream *stream, struct pmemstream_region *region, struct pmemstream_entry *entry,
		      const void *buf, size_t count, struct pmemstream_entry *new_entry)
{
	size_t entry_total_size = count + SPAN_ENTRY_METADATA_SIZE;
	size_t entry_total_size_span_aligned = ALIGN_UP(entry_total_size, sizeof(pmemstream_span_bytes));
	struct pmemstream_span_runtime region_srt = pmemstream_span_get_region_runtime(stream, region->offset);

	size_t offset = __atomic_fetch_add(&entry->offset, entry_total_size_span_aligned, __ATOMIC_RELEASE);

	/* region overflow (no space left) or offset outside of region. */
	if (offset + entry_total_size_span_aligned > region->offset + region_srt.total_size) {
		return -1;
	}
	/* offset outside of region */
	if (offset < region_srt.data_offset) {
		return -1;
	}

	if (new_entry) {
		new_entry->offset = offset;
	}

	pmemstream_span_create_entry(stream, offset, buf, count, util_popcount_memory(buf, count));

	return 0;
}

// returns pointer to the data of the entry
void *pmemstream_entry_data(struct pmemstream *stream, struct pmemstream_entry entry)
{
	struct pmemstream_span_runtime entry_srt = pmemstream_span_get_entry_runtime(stream, entry.offset);

	return pmemstream_offset_to_ptr(stream, entry_srt.data_offset);
}

// returns the size of the entry
size_t pmemstream_entry_length(struct pmemstream *stream, struct pmemstream_entry entry)
{
	struct pmemstream_span_runtime entry_srt = pmemstream_span_get_entry_runtime(stream, entry.offset);

	return entry_srt.entry.size;
}

/* Gets (or creates if missing) pointer to region_context associated with specified region. */
static int get_or_insert_region_context(struct pmemstream *stream, struct pmemstream_region region,
					struct pmemstream_region_context **container_handle)
{
	struct pmemstream_region_context *ctx = critnib_get(stream->region_contexts_container, region.offset);
	if (ctx) {
		if (container_handle) {
			*container_handle = ctx;
		}
		return 0;
	}

	int ret = -1;

	pthread_mutex_lock(&stream->region_contexts_container_lock);
	ctx = malloc(sizeof(*ctx));
	if (ctx) {
		*ctx = (struct pmemstream_region_context){0};
		ret = critnib_insert(stream->region_contexts_container, region.offset, ctx, 0 /* no update */);
	}
	pthread_mutex_unlock(&stream->region_contexts_container_lock);

	if (ret) {
		/* Insert failed, free the context. */
		free(ctx);

		if (ret == EEXIST) {
			/* Someone else inserted the region context - just get a pointer to it. */
			ctx = critnib_get(stream->region_contexts_container, region.offset);
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

int pmemstream_region_iterator_new(struct pmemstream_region_iterator **iterator, struct pmemstream *stream)
{
	struct pmemstream_region_iterator *iter = malloc(sizeof(*iter));
	iter->stream = stream;
	iter->region.offset = 0;

	*iterator = iter;

	return 0;
}

int pmemstream_region_iterator_next(struct pmemstream_region_iterator *it, struct pmemstream_region *region)
{
	struct pmemstream_span_runtime srt;

	while (it->region.offset < it->stream->usable_size) {
		srt = pmemstream_span_get_runtime(it->stream, it->region.offset);

		if (srt.type == PMEMSTREAM_SPAN_REGION) {
			*region = it->region;
			it->region.offset += srt.total_size;
			return 0;
		}

		/* if there are no more regions we should expect an empty span */
		assert(srt.type == PMEMSTREAM_SPAN_EMPTY);
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

static int pmemstream_entry_iterator_initialize(struct pmemstream_entry_iterator *iterator, struct pmemstream *stream,
						struct pmemstream_region region)
{
	struct pmemstream_span_runtime region_srt = pmemstream_span_get_region_runtime(stream, region.offset);
	struct pmemstream_entry_iterator iter;
	iter.offset = region_srt.data_offset;
	iter.region = region;
	iter.stream = stream;

	int ret = get_or_insert_region_context(stream, region, &iter.region_context);
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

	int ret = pmemstream_entry_iterator_initialize(iter, stream, region);
	if (ret) {
		free(iter);
		return ret;
	}

	*iterator = iter;

	return 0;
}

/* Performs stream recovery - clears all the data in the region after `tail` entry. */
static void recover_region(struct pmemstream *stream, struct pmemstream_region region,
			   struct pmemstream_region_context *region_context, struct pmemstream_entry tail)
{
	assert(region_context);
	assert(!region_context->recovered);

	struct pmemstream_span_runtime region_rt = pmemstream_span_get_region_runtime(stream, region.offset);
	size_t region_end_offset = region.offset + region_rt.total_size;
	size_t remaining_size = region_end_offset - tail.offset;
	pmemstream_span_create_empty(stream, tail.offset, remaining_size - SPAN_EMPTY_METADATA_SIZE);

	__atomic_store_n(&region_context->recovered, 1, __ATOMIC_RELAXED);
}

/* Advances entry iterator by one. Verifies entry integrity and recovers the region if necessary. */
int pmemstream_entry_iterator_next(struct pmemstream_entry_iterator *iter, struct pmemstream_region *region,
				   struct pmemstream_entry *user_entry)
{
	struct pmemstream_span_runtime srt = pmemstream_span_get_runtime(iter->stream, iter->offset);
	struct pmemstream_span_runtime region_srt =
		pmemstream_span_get_region_runtime(iter->stream, iter->region.offset);
	struct pmemstream_entry entry;
	entry.offset = iter->offset;

	// XXX: add test for NULL entry
	if (user_entry) {
		*user_entry = entry;
	}
	if (region) {
		*region = iter->region;
	}

	/* Make sure that we didn't go beyond region. */
	if (iter->offset >= iter->region.offset + region_srt.total_size) {
		return -1;
	}

	iter->offset += srt.total_size;

	/*
	 * Verify that all metadata and data fits inside the region - this should not fail unless stream was corrupted.
	 */
	assert(entry.offset + srt.total_size <= iter->region.offset + region_srt.total_size);

	int region_recovered = __atomic_load_n(&iter->region_context->recovered, __ATOMIC_RELAXED);

	if (region_recovered && srt.type == PMEMSTREAM_SPAN_EMPTY) {
		/* If we found last entry and region is already recovered, just return -1. */
		return -1;
	} else if (!region_recovered && validate_entry(iter->stream, entry) < 0) {
		/* If region was not yet recovered, validate that entry is correct. If there is any problem, recover the
		 * region. */
		recover_region(iter->stream, iter->region, iter->region_context, entry);
		return -1;
	}

	/* Region is already recovered, and we did not enounter end of the data yet - span must be a valid entry */
	assert(validate_entry(iter->stream, entry) == 0);

	return 0;
}

void pmemstream_entry_iterator_delete(struct pmemstream_entry_iterator **iterator)
{
	struct pmemstream_entry_iterator *iter = *iterator;

	free(iter);
	*iterator = NULL;
}
