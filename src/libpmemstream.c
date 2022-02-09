// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

/* Implementation of public C API */

#include "common/util.h"
#include "libpmemstream_internal.h"

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
	stream->memset(stream->header->signature, 0, PMEMSTREAM_SIGNATURE_SIZE,
		       PMEM2_F_MEM_NONTEMPORAL | PMEM2_F_MEM_NODRAIN);

	span_create_empty(stream, 0, stream->usable_size - SPAN_EMPTY_METADATA_SIZE);

	stream->header->stream_size = stream->stream_size;
	stream->header->block_size = stream->block_size;
	stream->persist(stream->header, sizeof(struct pmemstream_header));

	stream->memcpy(stream->header->signature, PMEMSTREAM_SIGNATURE, strlen(PMEMSTREAM_SIGNATURE),
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
	size_t stream_size = pmem2_map_get_size(map);
	if (stream_size > PTRDIFF_MAX) {
		return -1;
	}

	if (block_size == 0) {
		return -1;
	}

	/* XXX: change 64 to CACHELINE_SIZE */
	if (block_size % 64 != 0) {
		return -1;
	}

	if (!IS_POW2(block_size)) {
		return -1;
	}

	if (map == NULL) {
		return -1;
	}

	if (stream_size <= pmemstream_header_size_aligned(block_size)) {
		return -1;
	}

	if (pmemstream_usable_size(stream_size, block_size) <= SPAN_REGION_METADATA_SIZE) {
		return -1;
	}

	if (pmemstream_usable_size(stream_size, block_size) < block_size) {
		return -1;
	}

	return 0;
}

int pmemstream_from_map(struct pmemstream **stream, size_t block_size, struct pmem2_map *map)
{
	if (pmemstream_validate_sizes(block_size, map)) {
		return -1;
	}

	struct pmemstream *s = malloc(sizeof(struct pmemstream));
	if (!s) {
		return -1;
	}

	size_t stream_size = pmem2_map_get_size(map);
	size_t spans_offset = pmemstream_header_size_aligned(block_size);
	s->header = pmem2_map_get_address(map);
	s->spans = (span_bytes *)(((uint8_t *)pmem2_map_get_address(map)) + spans_offset);
	s->stream_size = stream_size;
	s->usable_size = pmemstream_usable_size(s->stream_size, block_size);
	s->block_size = block_size;
	s->memcpy = pmem2_get_memcpy_fn(map);
	s->memset = pmem2_get_memset_fn(map);
	s->persist = pmem2_get_persist_fn(map);
	s->flush = pmem2_get_flush_fn(map);
	s->drain = pmem2_get_drain_fn(map);

	if (pmemstream_is_initialized(s) != 0) {
		pmemstream_init(s);
	}

	s->region_runtimes_map = region_runtimes_map_new();
	if (!s->region_runtimes_map) {
		goto err;
	}

	*stream = s;
	return 0;

err:
	free(s);
	return -1;
}

void pmemstream_delete(struct pmemstream **stream)
{
	struct pmemstream *s = *stream;
	region_runtimes_map_destroy(s->region_runtimes_map);
	free(s);
	*stream = NULL;
}

// stream owns the region object - the user gets a reference, but it's not
// necessary to hold on to it and explicitly delete it.
/* XXX: add test for multiple regions, when supported */
int pmemstream_region_allocate(struct pmemstream *stream, size_t size, struct pmemstream_region *region)
{
	const uint64_t offset = 0;
	assert(offset % stream->block_size == 0);
	struct span_runtime srt = span_get_runtime(stream, offset);

	if (srt.type != SPAN_EMPTY) {
		return -1;
	}

	if (size == 0) {
		return -1;
	}

	size_t total_size = ALIGN_UP(size + SPAN_REGION_METADATA_SIZE, stream->block_size);
	if (total_size > srt.empty.size + SPAN_EMPTY_METADATA_SIZE)
		return -1;

	span_create_region(stream, offset, total_size - SPAN_REGION_METADATA_SIZE);
	region->offset = offset;

	/* XXX: use CACHELINE_SIZE instead of 64 */
	assert(((uintptr_t)pmemstream_offset_to_ptr(stream, span_get_runtime(stream, offset).data_offset)) % 64 == 0);

	return 0;
}

size_t pmemstream_region_size(struct pmemstream *stream, struct pmemstream_region region)
{
	struct span_runtime region_srt = span_get_region_runtime(stream, region.offset);

	return region_srt.region.size;
}

int pmemstream_region_free(struct pmemstream *stream, struct pmemstream_region region)
{
	struct span_runtime srt = span_get_runtime(stream, region.offset);

	if (srt.type != SPAN_REGION)
		return -1;

	span_create_empty(stream, 0, stream->usable_size - SPAN_EMPTY_METADATA_SIZE);

	region_runtimes_map_remove(stream->region_runtimes_map, region);

	return 0;
}

// returns pointer to the data of the entry
const void *pmemstream_entry_data(struct pmemstream *stream, struct pmemstream_entry entry)
{
	struct span_runtime entry_srt = span_get_entry_runtime(stream, entry.offset);

	return pmemstream_offset_to_ptr(stream, entry_srt.data_offset);
}

// returns the size of the entry
size_t pmemstream_entry_length(struct pmemstream *stream, struct pmemstream_entry entry)
{
	struct span_runtime entry_srt = span_get_entry_runtime(stream, entry.offset);

	return entry_srt.entry.size;
}

int pmemstream_region_runtime_initialize(struct pmemstream *stream, struct pmemstream_region region,
					 struct pmemstream_region_runtime **region_runtime)
{
	int ret = region_runtimes_map_get_or_create(stream->region_runtimes_map, region, region_runtime);
	if (ret) {
		return ret;
	}

	assert(*region_runtime);

	return region_runtime_initialize_clear_locked(stream, region, *region_runtime);
}

static size_t pmemstream_entry_total_size_aligned(size_t size)
{
	size_t entry_total_size = size + SPAN_ENTRY_METADATA_SIZE;
	return ALIGN_UP(entry_total_size, sizeof(span_bytes));
}

int pmemstream_reserve(struct pmemstream *stream, struct pmemstream_region region,
		       struct pmemstream_region_runtime *region_runtime, size_t size,
		       struct pmemstream_entry *reserved_entry, void **data_addr)
{
	size_t entry_total_size_span_aligned = pmemstream_entry_total_size_aligned(size);
	struct span_runtime region_srt = span_get_region_runtime(stream, region.offset);
	int ret = 0;

	if (!region_runtime) {
		ret = pmemstream_region_runtime_initialize(stream, region, &region_runtime);
		if (ret) {
			return ret;
		}
	}

	uint64_t offset = region_runtime_get_append_offset_acquire(region_runtime);
	assert(offset >= region_srt.data_offset);
	if (offset + entry_total_size_span_aligned > region.offset + region_srt.total_size) {
		return -1;
	}
	/* offset outside of region */
	if (offset < region_srt.data_offset) {
		return -1;
	}

	region_runtime_increase_append_offset(region_runtime, entry_total_size_span_aligned);

	reserved_entry->offset = offset;
	/* data is right after the entry metadata */
	*data_addr = (void *)span_offset_to_span_ptr(stream, offset + SPAN_ENTRY_METADATA_SIZE);

	return ret;
}

int pmemstream_publish(struct pmemstream *stream, struct pmemstream_region region,
		       struct pmemstream_region_runtime *region_runtime, const void *data, size_t size,
		       struct pmemstream_entry *reserved_entry)
{
	if (!region_runtime) {
		int ret = pmemstream_region_runtime_initialize(stream, region, &region_runtime);
		if (ret) {
			return ret;
		}
	}

	span_create_entry(stream, reserved_entry->offset, size, util_popcount_memory(data, size));
	region_runtime_increase_committed_offset(region_runtime, pmemstream_entry_total_size_aligned(size));

	return 0;
}

// synchronously appends data buffer to the end of the region
int pmemstream_append(struct pmemstream *stream, struct pmemstream_region region,
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

	stream->memcpy(reserved_dest, data, size, PMEM2_F_MEM_NODRAIN);
	span_create_entry_no_flush_data(stream, reserved_entry.offset, size, util_popcount_memory(data, size));
	region_runtime_increase_committed_offset(region_runtime, pmemstream_entry_total_size_aligned(size));

	if (new_entry) {
		new_entry->offset = reserved_entry.offset;
	}

	return 0;
}
