// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

/* Implementation of public C API */

#include "common/util.h"
#include "libpmemstream_internal.h"

#include <assert.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

static void pmemstream_span_create_empty(pmemstream_span_bytes *span, size_t data_size)
{
	assert((data_size & PMEMSTREAM_SPAN_TYPE_MASK) == 0);
	span[0] = data_size | PMEMSTREAM_SPAN_EMPTY;
}

static void pmemstream_span_create_entry(pmemstream_span_bytes *span, size_t data_size, size_t popcount)
{
	assert((data_size & PMEMSTREAM_SPAN_TYPE_MASK) == 0);
	span[0] = data_size | PMEMSTREAM_SPAN_ENTRY;
	span[1] = popcount;
}

static void pmemstream_span_create_region(pmemstream_span_bytes *span, size_t size)
{
	assert((size & PMEMSTREAM_SPAN_TYPE_MASK) == 0);
	span[0] = size | PMEMSTREAM_SPAN_REGION;
}

static struct pmemstream_span_runtime pmemstream_span_get_runtime(pmemstream_span_bytes *span)
{
	struct pmemstream_span_runtime sr;
	sr.type = span[0] & PMEMSTREAM_SPAN_TYPE_MASK;
	uint64_t extra = span[0] & PMEMSTREAM_SPAN_EXTRA_MASK;
	switch (sr.type) {
		case PMEMSTREAM_SPAN_EMPTY:
			sr.empty.size = extra;
			sr.data = (uint8_t *)(span + 1);
			sr.total_size = sr.empty.size + MEMBER_SIZE(pmemstream_span_runtime, empty);
			break;
		case PMEMSTREAM_SPAN_ENTRY:
			sr.entry.size = extra;
			sr.entry.popcount = span[1];
			sr.data = (uint8_t *)(span + 2);
			sr.total_size = sr.entry.size + MEMBER_SIZE(pmemstream_span_runtime, entry);
			break;
		case PMEMSTREAM_SPAN_REGION:
			sr.region.size = extra;
			sr.data = (uint8_t *)(span + 1);
			sr.total_size = sr.region.size + MEMBER_SIZE(pmemstream_span_runtime, region);
			break;
		default:
			abort();
	}

	return sr;
}

static int pmemstream_is_pmem_initialized(struct pmemstream *stream)
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

static void pmemstream_pmem_init(struct pmemstream *stream)
{
	memset(stream->data->header.signature, 0, PMEMSTREAM_SIGNATURE_SIZE);

	stream->data->header.stream_size = stream->stream_size;
	stream->data->header.block_size = stream->block_size;
	stream->persist(stream->data, sizeof(struct pmemstream_data));

	size_t metadata_size = MEMBER_SIZE(pmemstream_span_runtime, empty);
	pmemstream_span_create_empty(&stream->data->spans[0], stream->usable_size - metadata_size);
	stream->persist(&stream->data->spans[0], metadata_size);

	stream->memcpy(stream->data->header.signature, PMEMSTREAM_SIGNATURE, strlen(PMEMSTREAM_SIGNATURE), 0);
}

static pmemstream_span_bytes *pmemstream_get_span_for_offset(struct pmemstream *stream, size_t offset)
{
	return (pmemstream_span_bytes *)((uint8_t *)stream->data->spans + offset);
}

static uint64_t pmemstream_get_offset_for_span(struct pmemstream *stream, pmemstream_span_bytes *span)
{
	return (uint64_t)((uint64_t)span - (uint64_t)stream->data->spans);
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

	if (pmemstream_is_pmem_initialized(s) != 0) {
		pmemstream_pmem_init(s);
	}

	s->region_contexts_container = critnib_new();
	if (!s->region_contexts_container) {
		return -1;
	}

	*stream = s;

	return 0;
}

void pmemstream_delete(struct pmemstream **stream)
{
	struct pmemstream *s = *stream;
	critnib_delete(s->region_contexts_container);
	free(s);
	*stream = NULL;
}

// stream owns the region object - the user gets a reference, but it's not
// necessary to hold on to it and explicitly delete it.
int pmemstream_region_allocate(struct pmemstream *stream, size_t size, struct pmemstream_region *region)
{
	pmemstream_span_bytes *span = pmemstream_get_span_for_offset(stream, 0);
	struct pmemstream_span_runtime sr = pmemstream_span_get_runtime(span);

	if (sr.type != PMEMSTREAM_SPAN_EMPTY)
		return -1;

	size = ALIGN_UP(size, stream->block_size);

	size_t metadata_size = MEMBER_SIZE(pmemstream_span_runtime, region);
	pmemstream_span_create_region(span, size);
	stream->persist(&span[0], metadata_size);

	region->offset = 0;

	return 0;
}

int pmemstream_region_free(struct pmemstream *stream, struct pmemstream_region region)
{
	pmemstream_span_bytes *span = pmemstream_get_span_for_offset(stream, region.offset);
	struct pmemstream_span_runtime sr = pmemstream_span_get_runtime(span);

	if (sr.type != PMEMSTREAM_SPAN_REGION)
		return -1;

	size_t metadata_size = MEMBER_SIZE(pmemstream_span_runtime, empty);
	pmemstream_span_create_empty(&span[0], stream->usable_size - metadata_size);
	stream->persist(&span[0], metadata_size);

	struct pmemstream_region_context *ctx = critnib_remove(stream->region_contexts_container, region.offset);
	free(ctx);

	return 0;
}

// returns pointer to the data of the entry
void *pmemstream_entry_data(struct pmemstream *stream, struct pmemstream_entry entry)
{
	pmemstream_span_bytes *entry_span = pmemstream_get_span_for_offset(stream, entry.offset);
	struct pmemstream_span_runtime entry_sr = pmemstream_span_get_runtime(entry_span);
	assert(entry_sr.type == PMEMSTREAM_SPAN_ENTRY);

	return entry_sr.data;
}

// returns the size of the entry
size_t pmemstream_entry_length(struct pmemstream *stream, struct pmemstream_entry entry)
{
	pmemstream_span_bytes *entry_span = pmemstream_get_span_for_offset(stream, entry.offset);
	struct pmemstream_span_runtime entry_sr = pmemstream_span_get_runtime(entry_span);
	assert(entry_sr.type == PMEMSTREAM_SPAN_ENTRY);

	return entry_sr.entry.size;
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
	pmemstream_span_bytes *region_span;
	struct pmemstream_span_runtime region_sr;

	while (it->region.offset < it->stream->usable_size) {
		region_span = pmemstream_get_span_for_offset(it->stream, it->region.offset);
		region_sr = pmemstream_span_get_runtime(region_span);

		if (region_sr.type == PMEMSTREAM_SPAN_REGION) {
			*region = it->region;
			it->region.offset += region_sr.total_size;
			return 0;
		}

		assert(region_sr.type == PMEMSTREAM_SPAN_EMPTY);
		it->region.offset += region_sr.total_size;
	}

	return -1;
}

void pmemstream_region_iterator_delete(struct pmemstream_region_iterator **iterator)
{
	struct pmemstream_region_iterator *iter = *iterator;

	free(iter);
	*iterator = NULL;
}

static void pmemstream_entry_iterator_initialize(struct pmemstream_entry_iterator *iter, struct pmemstream *stream,
						 struct pmemstream_region region)
{
	iter->offset = region.offset + MEMBER_SIZE(pmemstream_span_runtime, region);
	iter->region = region;
	iter->stream = stream;
	iter->context_created = 0;
}

int pmemstream_entry_iterator_new(struct pmemstream_entry_iterator **iterator, struct pmemstream *stream,
				  struct pmemstream_region region)
{
	struct pmemstream_entry_iterator *iter = malloc(sizeof(*iter));
	if (!iter) {
		return -1; // XXX: proper error
	}

	pmemstream_entry_iterator_initialize(iter, stream, region);
	*iterator = iter;

	return 0;
}

/* Advances iterator by one. Returns -1 if outside of region, 0 otherwise. */
static int pmemstream_entry_iterator_next_internal(struct pmemstream_entry_iterator *iter,
						   struct pmemstream_region *region,
						   struct pmemstream_span_runtime *runtime,
						   struct pmemstream_entry *entry)
{
	pmemstream_span_bytes *entry_span = pmemstream_get_span_for_offset(iter->stream, iter->offset);

	assert(runtime);
	*runtime = pmemstream_span_get_runtime(entry_span);

	pmemstream_span_bytes *region_span = pmemstream_get_span_for_offset(iter->stream, iter->region.offset);
	struct pmemstream_span_runtime region_rt = pmemstream_span_get_runtime(region_span);

	uint64_t offset = pmemstream_get_offset_for_span(iter->stream, entry_span);

	if (entry) {
		entry->offset = offset;
	}
	if (region) {
		*region = iter->region;
	}

	if (iter->offset >= iter->region.offset + region_rt.total_size) {
		return -1;
	}

	// TODO: verify popcount
	iter->offset += runtime->total_size;

	return 0;
}

static int get_append_offset(struct pmemstream *stream, struct pmemstream_region region, struct pmemstream_entry *entry)
{
	assert(entry);

	struct pmemstream_entry_iterator iter;
	pmemstream_entry_iterator_initialize(&iter, stream, region);

	int ret;
	struct pmemstream_span_runtime runtime;
	struct pmemstream_entry it_entry; // XXX: move offset to pmemstream_span_runtime?
	while ((ret = pmemstream_entry_iterator_next_internal(&iter, NULL, &runtime, &it_entry)) == 0 &&
	       runtime.type == PMEMSTREAM_SPAN_ENTRY) {
	}
	if (ret) {
		return ret;
	}
	if (runtime.type != PMEMSTREAM_SPAN_EMPTY) {
		return -1;
	}

	*entry = it_entry;

	return 0;
}

struct critnib_context {
	struct pmemstream *stream;
	struct pmemstream_region region;
	struct pmemstream_entry entry;

	struct pmemstream_region_context *region_context;
};

/*
 * This function creates (or gets if already exists) region context for specific region.
 * It is called in critnib_emplace, under a lock.
 *
 * New region context is created as follows:
 *  1. Allocate new region_context structure.
 *  2. Fill region_context.append_offset:
 *     If PMEMSTREAM_INVALID_OFFSET is passed via entry.offset in critnib_context we need to find calculate it (it
 *     will be an offset just after last entry in the region). Otherwise, we just set region_context.append_offset
 *     to entry.offset.
 */
static int set_offset_to_entry(int exists, void **data, void *arg)
{
	struct critnib_context *ctx = (struct critnib_context *)arg;
	if (exists) {
		/* Element already exists, just return it. */
		ctx->region_context = *data;
		return 0;
	} else {
		/* Offset is not known yet, we need to calculate it. */
		if (ctx->entry.offset == PMEMSTREAM_INVALID_OFFSET) {
			int ret = get_append_offset(ctx->stream, ctx->region, &ctx->entry);
			if (ret) {
				ctx->region_context = NULL;
				return ret;
			}
		}

		/* ctx->entry.offset should now point to a valid location. */
		assert(ctx->entry.offset != PMEMSTREAM_INVALID_OFFSET);

		ctx->region_context = malloc(sizeof(*ctx->region_context));
		if (!ctx->region_context) {
			ctx->region_context = NULL;
			return -1;
		}

		__atomic_store_n(&ctx->region_context->append_offset, ctx->entry.offset, __ATOMIC_RELAXED);
		*data = ctx->region_context;

		return 0;
	}
}

/* Create (or get if already exists) region_context for specific region.
 * entry parameter specifies append_offset for that region_context. If it's PMEMSTREAM_INVALID_OFFSET this function
 * will calculate it itself. */
static int insert_or_get_region_context(struct pmemstream *stream, struct pmemstream_region region,
					struct pmemstream_entry entry,
					struct pmemstream_region_context **region_context)
{
	struct pmemstream_region_context *ctx = critnib_get(stream->region_contexts_container, region.offset);
	if (ctx) {
		if (region_context) {
			*region_context = ctx;
		}
		return 0;
	}

	struct critnib_context critnib_ctx;
	critnib_ctx.stream = stream;
	critnib_ctx.region = region;
	critnib_ctx.entry = entry;
	int ret = critnib_emplace(stream->region_contexts_container, region.offset, set_offset_to_entry, &critnib_ctx);
	if (ret) {
		return ret;
	}
	if (region_context) {
		*region_context = critnib_ctx.region_context;
	}

	return 0;
}

int pmemstream_entry_iterator_next(struct pmemstream_entry_iterator *iter, struct pmemstream_region *region,
				   struct pmemstream_entry *entry)
{
	struct pmemstream_span_runtime runtime;
	struct pmemstream_entry e;
	int ret = pmemstream_entry_iterator_next_internal(iter, region, &runtime, &e);
	if (ret) {
		return ret;
	}

	if (runtime.type == PMEMSTREAM_SPAN_EMPTY && !iter->context_created) {
		/* We found end of the stream - set append_offset if it's not already set and mark context as created.
		 */
		insert_or_get_region_context(iter->stream, iter->region, e, NULL);
		iter->context_created = 1;
	}

	if (entry) {
		*entry = e;
	}
	if (runtime.type == PMEMSTREAM_SPAN_ENTRY) {
		return 0;
	}

	return -1; // XXX: proper error, distinguish between corrupted and empty
}

int pmemstream_get_region_context(struct pmemstream *stream, struct pmemstream_region region,
				  struct pmemstream_region_context **ctx)
{
	struct pmemstream_entry entry_invalid;
	entry_invalid.offset = PMEMSTREAM_INVALID_OFFSET;
	return insert_or_get_region_context(stream, region, entry_invalid, ctx);
}

void pmemstream_entry_iterator_delete(struct pmemstream_entry_iterator **iterator)
{
	struct pmemstream_entry_iterator *iter = *iterator;

	free(iter);
	*iterator = NULL;
}

// synchronously appends data buffer to the end of the region
int pmemstream_append(struct pmemstream *stream, struct pmemstream_region region,
		      struct pmemstream_region_context *region_context, const void *buf, size_t count,
		      struct pmemstream_entry *new_entry)
{
	size_t entry_total_size = count + MEMBER_SIZE(pmemstream_span_runtime, entry);
	pmemstream_span_bytes *region_span = pmemstream_get_span_for_offset(stream, region.offset);
	struct pmemstream_span_runtime region_sr = pmemstream_span_get_runtime(region_span);

	if (!region_context) {
		int ret = pmemstream_get_region_context(stream, region, &region_context);
		if (ret) {
			return ret;
		}
	}

	size_t offset = __atomic_fetch_add(&region_context->append_offset, entry_total_size, __ATOMIC_RELEASE);
	assert(offset != PMEMSTREAM_INVALID_OFFSET);

	/* region overflow (no space left) or offset outside of region */
	if (offset + entry_total_size > region.offset + region_sr.total_size) {
		return -1;
	}
	/* offset outside of region */
	if (offset < region.offset + MEMBER_SIZE(pmemstream_span_runtime, region)) {
		return -1;
	}

	if (new_entry) {
		new_entry->offset = offset;
	}

	pmemstream_span_bytes *entry_span = pmemstream_get_span_for_offset(stream, offset);
	pmemstream_span_create_entry(entry_span, count, util_popcount_memory(buf, count));
	// TODO: for popcount, we also need to make sure that the memory is zeroed - maybe it can be done by bg thread?

	struct pmemstream_span_runtime entry_rt = pmemstream_span_get_runtime(entry_span);

	stream->memcpy(entry_rt.data, buf, count, PMEM2_F_MEM_NOFLUSH);
	stream->persist(&entry_span[0], entry_total_size);

	return 0;
}
