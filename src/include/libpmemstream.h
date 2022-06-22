// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

/* Public C API header */

#ifndef LIBPMEMSTREAM_H
#define LIBPMEMSTREAM_H

/*
 * XXX: put async functions into separate file...?
 * We wouldn't require then, miniasync installation every time.
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
struct pmemstream_entry_iterator;
struct pmemstream_region_iterator;
struct pmemstream_region_runtime;
struct pmemstream_region {
	uint64_t offset;
};

struct pmemstream_entry {
	uint64_t offset;
};

struct pmemstream_async_wait_data {
	struct pmemstream *stream;

	/* Timestamp to wait on. */
	uint64_t timestamp;

	/* first_timestamp and last_timestamp define a range of timestamp which this
	 * future will try to commit. processing_timestamp is timestamp which will be
	 * processed by next call to future_poll. */
	uint64_t first_timestamp;
	uint64_t processing_timestamp;
	uint64_t last_timestamp;
};

struct pmemstream_async_wait_output {
	int error_code;
};

FUTURE(pmemstream_async_wait_fut, struct pmemstream_async_wait_data, struct pmemstream_async_wait_output);

/* Creates new pmemstream instace from pmem2_map.
 * block_size defines alignment of regions - must be a power of 2 and multiple of CACHELINE size.
 */
int pmemstream_from_map(struct pmemstream **stream, size_t block_size, struct pmem2_map *map);
void pmemstream_delete(struct pmemstream **stream);

/* Allocates new region with specified size. Actual size might be bigger due to alignment requirements.
 *
 * Only fixed-sized regions are supported for now (pmemstream_region_allocate must always be called with the
 * same size).
 */
int pmemstream_region_allocate(struct pmemstream *stream, size_t size, struct pmemstream_region *region);

/* Frees previously allocated region. */
int pmemstream_region_free(struct pmemstream *stream, struct pmemstream_region region);

/* Returns region's size. It may be bigger from the size given to 'pmemstream_region_allocate'
 * due to an alignment (always up, never down). */
size_t pmemstream_region_size(struct pmemstream *stream, struct pmemstream_region region);

/* Returns current region's usable (free) size. It equals to: region end offset - region's append offset. */
size_t pmemstream_region_usable_size(struct pmemstream *stream, struct pmemstream_region region);

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
 * data is updated with a pointer to reserved space - this is a destination for, e.g., custom memcpy.
 *
 * It is not allowed to call pmemstream_reserve for the second time before calling pmemstream_publish.
 * */
int pmemstream_reserve(struct pmemstream *stream, struct pmemstream_region region,
		       struct pmemstream_region_runtime *region_runtime, size_t size,
		       struct pmemstream_entry *reserved_entry, void **data);

/* Publish previously custom-written entry.
 * After calling pmemstream_reserve and writing/memcpy'ing data into a reserved_entry, it's required
 * to call this function for setting proper entry's metadata and persist the data.
 *
 * size of the entry have to match the previous reservation and the actual size of the data written by user.
 */
int pmemstream_publish(struct pmemstream *stream, struct pmemstream_region region,
		       struct pmemstream_region_runtime *region_runtime, struct pmemstream_entry entry, size_t size);

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

/* Asynchronous publish. Entry is marked as ready for commit.
 *
 * There is no guarantee whether data is visible by iterators or persisted after this call.
 * To commit (and make the data visible to iterators) or persist the data use: pmemstream_async_wait_committed or
 * pmemstream_async_wait_persisted.
 */
int pmemstream_async_publish(struct pmemstream *stream, struct pmemstream_region region,
			     struct pmemstream_region_runtime *region_runtime, struct pmemstream_entry entry,
			     size_t size);

/* Asynchronous append. Appends data to the region and marks it as ready for commit.
 *
 * There is no guarantee whether data is visible by iterators or persisted after this call.
 * To commit (and make the data visible to iterators) or persist the data use: pmemstream_async_wait_committed or
 * pmemstream_async_wait_persisted.
 */
int pmemstream_async_append(struct pmemstream *stream, struct vdm *vdm, struct pmemstream_region region,
			    struct pmemstream_region_runtime *region_runtime, const void *data, size_t size,
			    struct pmemstream_entry *new_entry);

/* Return committed timestamp. All entries with timestamps less than or equal to that timestamp can be treated as
 * committed. */
uint64_t pmemstream_committed_timestamp(struct pmemstream *stream);

/* Return persisted timestamp. All entries with timestamps less than or equal to that timestamp can be treated as
 * persisted. It is guaranteed to be less than or equal to committed timestamp.
 */
uint64_t pmemstream_persisted_timestamp(struct pmemstream *stream);

/* Returns future for committing/persisting all entries up to specified timestamp.
 * Future must be polled until completion.
 *
 * Data which is committed but not yet persisted will be visible for iterators but might not be reachable after
 * application restart.
 *
 * XXX: possible extra variants:
 * - pmemstream_wait_commited/persisted (blocking)
 * - pmemstream_process_commited/persisted (process as many commited/persisted ops as possible without blocking)
 */
struct pmemstream_async_wait_fut pmemstream_async_wait_committed(struct pmemstream *stream, uint64_t timestamp);
struct pmemstream_async_wait_fut pmemstream_async_wait_persisted(struct pmemstream *stream, uint64_t timestamp);

/* Returns pointer to the data of the entry. Assumes that 'entry' points to a valid entry. */
const void *pmemstream_entry_data(struct pmemstream *stream, struct pmemstream_entry entry);

/* Returns the size of the entry. Assumes that 'entry' points to a valid entry. */
size_t pmemstream_entry_length(struct pmemstream *stream, struct pmemstream_entry entry);

/* Returns timestamp related with entry */
uint64_t pmemstream_entry_timestamp(struct pmemstream *stream, struct pmemstream_entry entry);

/* Creates new region iterator
 *
 * Default state is undefined: every new iterator should be moved to first element in the stream.
 * Returns -1 if there is an error.
 * See also: `pmemstream_region_iterator_seek_first`
 */
int pmemstream_region_iterator_new(struct pmemstream_region_iterator **iterator, struct pmemstream *stream);

/* Checks that region iterator is in valid state.
 *
 * Returns 0 when iterator is valid, and error code otherwise.
 */
int pmemstream_region_iterator_is_valid(struct pmemstream_region_iterator *iterator);

/* Set region iterator to first region.
 *
 * Function sets iterator to first region, or sets iterator to invalid region.
 */
void pmemstream_region_iterator_seek_first(struct pmemstream_region_iterator *iterator);

/* Moves iterator to next region.
 *
 * Moves to next region, or sets iterator to invalid region.
 */
void pmemstream_region_iterator_next(struct pmemstream_region_iterator *iterator);

/* Get region from region iterator.
 *
 * Function returns region that iterator points to, or invalid region otherwise.
 */
struct pmemstream_region pmemstream_region_iterator_get(struct pmemstream_region_iterator *iterator);

/* Release region iterator resources. */
void pmemstream_region_iterator_delete(struct pmemstream_region_iterator **iterator);

/* Creates new entry iterator
 *
 * Default state is undefined: every new iterator should be moved to first element in the stream.
 * Returns -1 if there is an error.
 * See also: `pmemstream_entry_iterator_seek_first`
 *
 * Entry iterator will iterate over all commited (but not necessarily persisted) entries.
 */
int pmemstream_entry_iterator_new(struct pmemstream_entry_iterator **iterator, struct pmemstream *stream,
				  struct pmemstream_region region);

/* Checks that entry iterator is in valid state.
 *
 * Returns 0 when iterator is valid, and error code otherwise.
 */
int pmemstream_entry_iterator_is_valid(struct pmemstream_entry_iterator *iterator);

/* Moves iterator to next region.
 *
 * Moves to next entry if possible. Calling `pmemstream_entry_iterator_next()`
 * on invalid entry is undefined behaviour, so it should always be called after
 * `pmemstream_entry_iterator_is_valid()`
 * ```
 *    if(pmemstream_entry_iterator_is_valid(it) == 0)
 *		pmemstream_entry_iterator_next(it);
 * ```
 */
void pmemstream_entry_iterator_next(struct pmemstream_entry_iterator *iterator);

/* Set entry iterator to first entry.
 *
 * Function sets entry iterator to first entry in region, or sets iterator to invalid entry.
 */
void pmemstream_entry_iterator_seek_first(struct pmemstream_entry_iterator *iterator);

/* Get entry from entry iterator.
 *
 * Function returns entry that iterator points.
 */
struct pmemstream_entry pmemstream_entry_iterator_get(struct pmemstream_entry_iterator *iterator);

/* Release region iterator resources. */
void pmemstream_entry_iterator_delete(struct pmemstream_entry_iterator **iterator);

#ifdef __cplusplus
} /* end extern "C" */
#endif
#endif /* LIBPMEMSTREAM_H */
