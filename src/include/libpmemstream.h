// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

/* Public C API header */

#ifndef LIBPMEMSTREAM_H
#define LIBPMEMSTREAM_H

/*
 * XXX: put async functions into separate file...?
 * We wouldn't require then, miniasync installation everytime.
 */
#include <libminiasync.h>
#include <libpmem2.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct pmemstream;
struct pmemstream_tx;
struct pmemstream_entry_iterator;
struct pmemstream_region_iterator;
struct pmemstream_region_runtime;
struct pmemstream_region {
	uint64_t offset;
};

struct pmemstream_entry {
	uint64_t offset;
};

/* async publish data, filled with data from user and from pmemstream_reserve */
struct pmemstream_async_publish_data {
	struct pmemstream *stream;
	struct pmemstream_region region;
	struct pmemstream_region_runtime *region_runtime;
	const void *data;
	size_t size;
	struct pmemstream_entry reserved_entry;
};

/* async publish output */
struct pmemstream_async_publish_output {
	int error_code;
};

FUTURE(pmemstream_async_publish_fut, struct pmemstream_async_publish_data, struct pmemstream_async_publish_output);

/* async append data - two chained futures needed to complete append */
struct pmemstream_async_append_data {
	FUTURE_CHAIN_ENTRY(struct vdm_operation_future, memcpy);
	FUTURE_CHAIN_ENTRY(struct pmemstream_async_publish_fut, publish);
};

/* async append returns new entry's offset on success (error_code == 0) */
struct pmemstream_async_append_output {
	int error_code;
	struct pmemstream_entry new_entry;
};

FUTURE(pmemstream_async_append_fut, struct pmemstream_async_append_data, struct pmemstream_async_append_output);

// manages lifecycle of the stream. Can be based on top of a raw pmem2_map
// or a pmemset (TBD).
// block_size defines alignment of regions - must be a power of 2 and multiple of CACHELINE size.
int pmemstream_from_map(struct pmemstream **stream, size_t block_size, struct pmem2_map *map);
void pmemstream_delete(struct pmemstream **stream);

/* Gets the last committed/persisted timestamp in pmemstream */
uint64_t pmemstream_committed_timestamp(struct pmemstream *stream);
uint64_t pmemstream_persisted_timestamp(struct pmemstream *stream);

// stream owns the region object - the user gets a reference, but it's not
// necessary to hold on to it and explicitly delete it.
// Only fixed-sized regions are supported for now (pmemstream_region_allocate must always be called with the
// same size).
int pmemstream_region_allocate(struct pmemstream *stream, size_t size, struct pmemstream_region *region);

int pmemstream_region_free(struct pmemstream *stream, struct pmemstream_region region);

size_t pmemstream_region_size(struct pmemstream *stream, struct pmemstream_region region);

/* Returns pointer to pmemstream_region_runtime. The runtime is managed by libpmemstream - user does not
 * have to explicitly delete/free it. Runtime becomes invalid after corresponding region is freed.
 *
 * Call to this function might be expensive. If it is not called explicitly, pmemstream will call it
 * inside append/reserve.
 */
int pmemstream_region_runtime_initialize(struct pmemstream *stream, struct pmemstream_region region,
					 struct pmemstream_region_runtime **runtime);

/* Reserve space (for a future, custom write) of a given size, in a region at offset pointed by region_runtime.
 * Entry's data have to be copied into reserved space by the user and then published using pmemstream_publish.
 * For regular usage, pmemstream_append should be simpler and safer to use and provide better performance.
 *
 * region_runtime is an optional parameter which can be obtained from pmemstream_region_runtime_initialize.
 * If it's NULL, it will be obtained from its internal structures (which might incur overhead).
 * reserved_entry is updated with offset of the reserved entry.
 * data is updated with a pointer to reserved space - this is a destination for, e.g., custom memcpy. */
int pmemstream_reserve(struct pmemstream *stream, struct pmemstream_region region,
		       struct pmemstream_region_runtime *region_runtime, size_t size,
		       struct pmemstream_entry *reserved_entry, void **data);

/* Publish previously custom-written entry.
 * After calling pmemstream_reserve and writing/memcpy'ing data into a reserved_entry, it's required
 * to call this function for setting proper entry's metadata and persist the data.
 *
 * *data has to hold the same data as they were written by user (e.g. in custom memcpy).
 * size of the entry have to match the previous reservation and the actual size of the data written by user. */
int pmemstream_publish(struct pmemstream *stream, struct pmemstream_region region,
		       struct pmemstream_region_runtime *region_runtime, const void *data, size_t size,
		       struct pmemstream_entry reserved_entry);

/* Synchronously appends data buffer after last valid entry in region.
 * Fails if no space is available.
 *
 * region_runtime is an optional parameter which can be obtained from pmemstream_region_runtime_initialize.
 * If it's NULL, it will be obtained from its internal structures (which might incur overhead).
 *
 * data is a pointer to data to be appended
 * size is size of the data to be appended
 *
 * new_entry is an optional pointer. On success, it will contain information about position of newly
 * appended entry.
 */
int pmemstream_append(struct pmemstream *stream, struct pmemstream_region region,
		      struct pmemstream_region_runtime *region_runtime, const void *data, size_t size,
		      struct pmemstream_entry *new_entry);

/* asynchronous publish, using libminiasync */
struct pmemstream_async_publish_fut pmemstream_async_publish(struct pmemstream *stream, struct pmemstream_region region,
							     struct pmemstream_region_runtime *region_runtime,
							     const void *data, size_t size,
							     struct pmemstream_entry reserved_entry);

/* asynchronous append, using libminiasync */
struct pmemstream_async_append_fut pmemstream_async_append(struct pmemstream *stream, struct vdm *vdm,
							   struct pmemstream_region region,
							   struct pmemstream_region_runtime *region_runtime,
							   const void *data, size_t size);

/* Returns pointer to the data of the entry. Assumes that 'entry' points to a valid entry. */
const void *pmemstream_entry_data(struct pmemstream *stream, struct pmemstream_entry entry);

/* Returns the size of the entry. Assumes that 'entry' points to a valid entry. */
size_t pmemstream_entry_length(struct pmemstream *stream, struct pmemstream_entry entry);

/* Creates new region iterator
 *
 * Default state is undefined: every new iterator should be moved to first element in the stream.
 * Returns -1 if there is an error.
 * See also: `pmemstream_region_iterator_seek_first`
 */
int pmemstream_region_iterator_new(struct pmemstream_region_iterator **iterator, struct pmemstream *stream);

/* Checks that iterator is in valid state.
 *
 * Returns 0 when iterator is valid, and error code otherwise.
 */
int pmemstream_region_iterator_is_valid(struct pmemstream_region_iterator *iterator);

/* Set iterator to first region.
 *
 * Function sets iterator to first region, or sets iterator to invalid region.
 */
void pmemstream_region_iterator_seek_first(struct pmemstream_region_iterator *iterator);

/* Moves iterator to next region.
 *
 * Moves to next region, or sets iterator to invalid region.
 */
void pmemstream_region_iterator_next(struct pmemstream_region_iterator *iterator);

/* Get region from iterator.
 *
 * Function returns region that iterator points to, or invalid region otherwise.
 */
struct pmemstream_region pmemstream_region_iterator_get(struct pmemstream_region_iterator *iterator);

/* Release iterator resources. */
void pmemstream_region_iterator_delete(struct pmemstream_region_iterator **iterator);

int pmemstream_entry_iterator_new(struct pmemstream_entry_iterator **iterator, struct pmemstream *stream,
				  struct pmemstream_region region);

/* If this function succeeds, entry points to a valid element in the stream. Otherwise, it points to a
 * last valid entry or to a beggining of region if there are no valid entries. */
int pmemstream_entry_iterator_next(struct pmemstream_entry_iterator *iterator, struct pmemstream_region *region,
				   struct pmemstream_entry *entry);

void pmemstream_entry_iterator_delete(struct pmemstream_entry_iterator **iterator);

#ifdef __cplusplus
} /* end extern "C" */
#endif
#endif /* LIBPMEMSTREAM_H */
