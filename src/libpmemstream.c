// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

/* Implementation of public C API */

#include "common/util.h"
#include "libpmemstream_internal.h"

#include <assert.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

static void pmemstream_span_create_empty(struct pmemstream *stream, pmemstream_span_bytes *span, size_t data_size)
{
	assert((data_size & PMEMSTREAM_SPAN_TYPE_MASK) == 0);
	span[0] = data_size | PMEMSTREAM_SPAN_EMPTY;
	stream->memset(&span[1], 0, data_size, PMEM2_F_MEM_NONTEMPORAL);
}

/* Creates entry span with data_size and popcount. */
static void pmemstream_span_create_entry(pmemstream_span_bytes *span, size_t data_size, size_t popcount)
{
	assert((data_size & PMEMSTREAM_SPAN_TYPE_MASK) == 0);
	span[0] = data_size | PMEMSTREAM_SPAN_ENTRY;
	span[1] = popcount;
}

/* Creates just clean entry span with only type set.
 * This is required if we don't know the popcount/size, but still want to set proper type
 * (and have proper span's structure). */
static void pmemstream_span_create_entry_clean(pmemstream_span_bytes *span)
{
	span[0] = PMEMSTREAM_SPAN_ENTRY;
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
	pmemstream_span_create_empty(stream, &stream->data->spans[0], stream->usable_size - metadata_size);
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
	pmemstream_span_create_empty(stream, &span[0], stream->usable_size - metadata_size);
	stream->persist(&span[0], metadata_size);

	return 0;
}

/* Reserve space (for a future, custom write) of a given size, in a region at offset pointed by entry.
 * Entry's data have to be copied into reserved space by the user and then published using pmemstream_publish.
 * For regular usage, pmemstream_append should be simpler and safer to use and provide better performance.
 *
 * entry is updated with new offset, for a next entry.
 * reserved_entry is updated with offset of the reserved entry.
 * data is updated with a pointer to reserved space - this is a destination for, e.g., custom memcpy. */
int pmemstream_reserve(struct pmemstream *stream, struct pmemstream_region *region, struct pmemstream_entry *entry,
		       size_t size, struct pmemstream_entry *reserved_entry, void **data)
{
	size_t entry_total_size = size + MEMBER_SIZE(pmemstream_span_runtime, entry);

	pmemstream_span_bytes *region_span = pmemstream_get_span_for_offset(stream, region->offset);
	struct pmemstream_span_runtime region_sr = pmemstream_span_get_runtime(region_span);

	size_t offset = __atomic_fetch_add(&entry->offset, entry_total_size, __ATOMIC_RELEASE);

	/* region overflow (no space left) or offset outside of region */
	if (offset + entry_total_size > region->offset + region_sr.total_size) {
		return -1;
	}
	/* offset outside of region */
	if (offset < region->offset + MEMBER_SIZE(pmemstream_span_runtime, region)) {
		return -1;
	}

	pmemstream_span_bytes *entry_span = pmemstream_get_span_for_offset(stream, offset);
	/* The line below assured to get entry-type span, but it also sets 0 in popcount, which may affect cache.
	 * Workaround is to use special function only setting the entry type (and no size, nor popcount).
	 * XXX: benchmark if creating entry with 0's affects the further non-temporal data memcpy */
	// pmemstream_span_create_entry(entry_span, 0, 0);
	pmemstream_span_create_entry_clean(entry_span);
	struct pmemstream_span_runtime entry_sr = pmemstream_span_get_runtime(entry_span);

	reserved_entry->offset = offset;
	*data = entry_sr.data;
	return 0;
}

/* Publish previously custom-written entry.
 * After calling pmemstream_reserve and writing/memcpy'ing data into a reserved space it's required
 * to calculate the checksum (based on the buf and size).
 *
 * entry is not updated in any way.
 * buf holds data and has to be the same as the actual data written by user.
 * size of the entry have to match the previous reservation and the actual size of the data written by user.
 * flags may be used to adjust behavior of persisting the data. */
int pmemstream_publish(struct pmemstream *stream, struct pmemstream_region *region, struct pmemstream_entry *entry,
		       const void *buf, size_t size, int flags)
{
	size_t entry_total_size = size + MEMBER_SIZE(pmemstream_span_runtime, entry);

	/* XXX: check/assert if the size, we try to write, fits the reserved space? */
	pmemstream_span_bytes *entry_span = pmemstream_get_span_for_offset(stream, entry->offset);

	/* save size and popcount in entry_span; data (from user) should be already right next to it */
	pmemstream_span_create_entry(entry_span, size, util_popcount_memory(buf, size));

	if (flags & PMEMSTREAM_PUBLISH_PERSIST) {
		/* persist whole entry: size, popcount, and data */
		stream->persist(&entry_span[0], entry_total_size);
	}

	return 0;
}

/* Synchronously appends data buffer within a region at offset pointed by entry.
 * entry is updated with new offset; (optionally) offset of appended entry is saved in new_entry. */
int pmemstream_append(struct pmemstream *stream, struct pmemstream_region *region, struct pmemstream_entry *entry,
		      const void *buf, size_t size, struct pmemstream_entry *new_entry)
{
	struct pmemstream_entry reserved_entry;
	void *reserved_dest;
	int ret = pmemstream_reserve(stream, region, entry, size, &reserved_entry, &reserved_dest);
	if (ret != 0) {
		return ret;
	}

	stream->memcpy(reserved_dest, buf, size, PMEM2_F_MEM_NONTEMPORAL);
	ret = pmemstream_publish(stream, region, &reserved_entry, buf, size, 0);
	if (ret != 0) {
		return ret;
	}

	if (new_entry) {
		new_entry->offset = reserved_entry.offset;
	}

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

int pmemstream_entry_iterator_new(struct pmemstream_entry_iterator **iterator, struct pmemstream *stream,
				  struct pmemstream_region region)
{
	struct pmemstream_entry_iterator *iter = malloc(sizeof(*iter));
	iter->offset = region.offset + MEMBER_SIZE(pmemstream_span_runtime, region);
	iter->region = region;
	iter->stream = stream;

	*iterator = iter;

	return 0;
}

/* XXX: add desc
 *
 * Iteration over entries will not work properly if pmemstream_reserve was called without pmemstream_publish. */
int pmemstream_entry_iterator_next(struct pmemstream_entry_iterator *iter, struct pmemstream_region *region,
				   struct pmemstream_entry *entry)
{
	pmemstream_span_bytes *entry_span = pmemstream_get_span_for_offset(iter->stream, iter->offset);
	struct pmemstream_span_runtime rt = pmemstream_span_get_runtime(entry_span);

	pmemstream_span_bytes *region_span = pmemstream_get_span_for_offset(iter->stream, iter->region.offset);
	struct pmemstream_span_runtime region_rt = pmemstream_span_get_runtime(region_span);

	if (entry) {
		entry->offset = pmemstream_get_offset_for_span(iter->stream, entry_span);
	}
	if (region) {
		*region = iter->region;
	}

	if (iter->offset >= iter->region.offset + region_rt.total_size) {
		return -1;
	}

	// TODO: verify popcount
	iter->offset += rt.total_size;

	if (rt.type == PMEMSTREAM_SPAN_ENTRY) {
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
