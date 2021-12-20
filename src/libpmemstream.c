// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

/* Implementation of public C API */

#include "common/util.h"
#include "libpmemstream_internal.h"

#include <assert.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

static pmemstream_span_bytes *pmemstream_get_span_for_offset(struct pmemstream *stream, size_t offset)
{
	return (pmemstream_span_bytes *)((uint8_t *)stream->data->spans + offset);
}

static void pmemstream_span_create_empty(struct pmemstream *stream, uint64_t offset, size_t data_size)
{
	pmemstream_span_bytes *span = pmemstream_get_span_for_offset(stream, offset);
	assert((data_size & PMEMSTREAM_SPAN_TYPE_MASK) == 0);
	span[0] = data_size | PMEMSTREAM_SPAN_EMPTY;
	stream->memset(&span[1], 0, data_size, PMEM2_F_MEM_NONTEMPORAL);
}

static void pmemstream_span_create_entry(struct pmemstream *stream, uint64_t offset, size_t data_size, size_t popcount)
{
	pmemstream_span_bytes *span = pmemstream_get_span_for_offset(stream, offset);
	assert((data_size & PMEMSTREAM_SPAN_TYPE_MASK) == 0);
	span[0] = data_size | PMEMSTREAM_SPAN_ENTRY;
	span[1] = popcount;
}

static void pmemstream_span_create_region(struct pmemstream *stream, uint64_t offset, size_t size)
{
	pmemstream_span_bytes *span = pmemstream_get_span_for_offset(stream, offset);
	assert((size & PMEMSTREAM_SPAN_TYPE_MASK) == 0);
	span[0] = size | PMEMSTREAM_SPAN_REGION;
}

static struct pmemstream_span_runtime pmemstream_span_get_runtime(struct pmemstream *stream, uint64_t offset)
{
	pmemstream_span_bytes *span = pmemstream_get_span_for_offset(stream, offset);
	struct pmemstream_span_runtime sr;
	sr.type = span[0] & PMEMSTREAM_SPAN_TYPE_MASK;
	uint64_t extra = span[0] & PMEMSTREAM_SPAN_EXTRA_MASK;
	switch (sr.type) {
		case PMEMSTREAM_SPAN_EMPTY:
			sr.empty.size = extra;
			sr.data_offset = offset + MEMBER_SIZE(pmemstream_span_runtime, empty);
			sr.total_size = sr.empty.size + MEMBER_SIZE(pmemstream_span_runtime, empty);
			break;
		case PMEMSTREAM_SPAN_ENTRY:
			sr.entry.size = extra;
			sr.entry.popcount = span[1];
			sr.data_offset = offset + MEMBER_SIZE(pmemstream_span_runtime, entry);
			sr.total_size = sr.entry.size + MEMBER_SIZE(pmemstream_span_runtime, entry);
			break;
		case PMEMSTREAM_SPAN_REGION:
			sr.region.size = extra;
			sr.data_offset = offset + MEMBER_SIZE(pmemstream_span_runtime, region);
			sr.total_size = sr.region.size + MEMBER_SIZE(pmemstream_span_runtime, region);
			break;
		default:
			abort();
	}

	return sr;
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
	memset(stream->data->header.signature, 0, PMEMSTREAM_SIGNATURE_SIZE);

	stream->data->header.stream_size = stream->stream_size;
	stream->data->header.block_size = stream->block_size;
	stream->persist(stream->data, sizeof(struct pmemstream_data));

	size_t metadata_size = MEMBER_SIZE(pmemstream_span_runtime, empty);
	pmemstream_span_create_empty(stream, 0, stream->usable_size - metadata_size);
	stream->persist(&stream->data->spans[0], metadata_size);

	stream->memcpy(stream->data->header.signature, PMEMSTREAM_SIGNATURE, strlen(PMEMSTREAM_SIGNATURE), 0);
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

	*stream = s;

	return 0;
}

void pmemstream_delete(struct pmemstream **stream)
{
	struct pmemstream *s = *stream;
	free(s);
	*stream = NULL;
}

// stream owns the region object - the user gets a reference, but it's not
// necessary to hold on to it and explicitly delete it.
int pmemstream_region_allocate(struct pmemstream *stream, size_t size, struct pmemstream_region *region)
{
	const uint64_t offset = 0;
	struct pmemstream_span_runtime sr = pmemstream_span_get_runtime(stream, offset);

	if (sr.type != PMEMSTREAM_SPAN_EMPTY)
		return -1;

	size = ALIGN_UP(size, stream->block_size);

	size_t metadata_size = MEMBER_SIZE(pmemstream_span_runtime, region);
	pmemstream_span_create_region(stream, offset, size);
	stream->persist(pmemstream_get_span_for_offset(stream, offset), metadata_size);

	region->offset = offset;

	return 0;
}

int pmemstream_region_free(struct pmemstream *stream, struct pmemstream_region region)
{
	struct pmemstream_span_runtime sr = pmemstream_span_get_runtime(stream, region.offset);

	if (sr.type != PMEMSTREAM_SPAN_REGION)
		return -1;

	size_t metadata_size = MEMBER_SIZE(pmemstream_span_runtime, empty);
	pmemstream_span_create_empty(stream, region.offset, stream->usable_size - metadata_size);
	stream->persist(pmemstream_get_span_for_offset(stream, region.offset), metadata_size);

	return 0;
}

// synchronously appends data buffer to the end of the region
int pmemstream_append(struct pmemstream *stream, struct pmemstream_region *region, struct pmemstream_entry *entry,
		      const void *buf, size_t count, struct pmemstream_entry *new_entry)
{
	size_t entry_total_size = count + MEMBER_SIZE(pmemstream_span_runtime, entry);
	struct pmemstream_span_runtime region_sr = pmemstream_span_get_runtime(stream, region->offset);

	size_t offset = __atomic_fetch_add(&entry->offset, entry_total_size, __ATOMIC_RELEASE);

	/* region overflow (no space left) or offset outside of region */
	if (offset + entry_total_size > region->offset + region_sr.total_size) {
		return -1;
	}
	/* offset outside of region */
	if (offset < region_sr.data_offset) {
		return -1;
	}

	if (new_entry) {
		new_entry->offset = offset;
	}

	pmemstream_span_create_entry(stream, offset, count, util_popcount_memory(buf, count));
	// TODO: for popcount, we also need to make sure that the memory is zeroed - maybe it can be done by bg thread?

	struct pmemstream_span_runtime entry_rt = pmemstream_span_get_runtime(stream, offset);

	stream->memcpy(pmemstream_get_span_for_offset(stream, entry_rt.data_offset), buf, count, PMEM2_F_MEM_NOFLUSH);
	stream->persist(pmemstream_get_span_for_offset(stream, offset), entry_total_size);

	return 0;
}

// returns pointer to the data of the entry
void *pmemstream_entry_data(struct pmemstream *stream, struct pmemstream_entry entry)
{
	struct pmemstream_span_runtime entry_sr = pmemstream_span_get_runtime(stream, entry.offset);
	assert(entry_sr.type == PMEMSTREAM_SPAN_ENTRY);

	return pmemstream_get_span_for_offset(stream, entry_sr.data_offset);
}

// returns the size of the entry
size_t pmemstream_entry_length(struct pmemstream *stream, struct pmemstream_entry entry)
{
	struct pmemstream_span_runtime entry_sr = pmemstream_span_get_runtime(stream, entry.offset);
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
	struct pmemstream_span_runtime region_sr;

	while (it->region.offset < it->stream->usable_size) {
		region_sr = pmemstream_span_get_runtime(it->stream, it->region.offset);

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

int pmemstream_entry_iterator_new(struct pmemstream_entry_iterator **iterator, struct pmemstream *stream,
				  struct pmemstream_region region)
{
	struct pmemstream_span_runtime rt = pmemstream_span_get_runtime(stream, region.offset);
	struct pmemstream_entry_iterator *iter = malloc(sizeof(*iter));
	iter->offset = rt.data_offset;
	iter->region = region;
	iter->stream = stream;

	*iterator = iter;

	return 0;
}

int pmemstream_entry_iterator_next(struct pmemstream_entry_iterator *iter, struct pmemstream_region *region,
				   struct pmemstream_entry *entry)
{
	struct pmemstream_span_runtime entry_rt = pmemstream_span_get_runtime(iter->stream, iter->offset);
	struct pmemstream_span_runtime region_rt = pmemstream_span_get_runtime(iter->stream, iter->region.offset);

	if (entry) {
		entry->offset = iter->offset;
	}
	if (region) {
		*region = iter->region;
	}

	if (iter->offset >= iter->region.offset + region_rt.total_size) {
		return -1;
	}

	// TODO: verify popcount
	iter->offset += entry_rt.total_size;

	if (entry_rt.type == PMEMSTREAM_SPAN_ENTRY) {
		return 0;
	}

	return -1;
}

void pmemstream_entry_iterator_delete(struct pmemstream_entry_iterator **iterator)
{
	struct pmemstream_entry_iterator *iter = *iterator;

	free(iter);
	*iterator = NULL;
}
