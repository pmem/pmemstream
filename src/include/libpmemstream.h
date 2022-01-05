// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

/* Public C API header */

#ifndef LIBPMEMSTREAM_H
#define LIBPMEMSTREAM_H

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
struct pmemstream_region_context;
struct pmemstream_region {
	uint64_t offset;
};

struct pmemstream_entry {
	uint64_t offset;
};

// manages lifecycle of the stream. Can be based on top of a raw pmem2_map
// or a pmemset (TBD).
int pmemstream_from_map(struct pmemstream **stream, size_t block_size, struct pmem2_map *map);
void pmemstream_delete(struct pmemstream **stream);

// stream owns the region object - the user gets a reference, but it's not
// necessary to hold on to it and explicitly delete it.
int pmemstream_region_allocate(const struct pmemstream *stream, size_t size, struct pmemstream_region *region);

int pmemstream_region_free(const struct pmemstream *stream, struct pmemstream_region region);

size_t pmemstream_region_size(struct pmemstream *stream, struct pmemstream_region region);

/* Returns pointer to pmemstream_region_context. The context is managed by libpmemstream - user does not
 * have to explicitly delete/free it. Context becomes invalid after corresponding region is freed. */
int pmemstream_get_region_context(struct pmemstream *stream, struct pmemstream_region region,
				  struct pmemstream_region_context **ctx);

/* Reserve space (for a future, custom write) of a given size, in a region at offset pointed by region_context.
 * Entry's data have to be copied into reserved space by the user and then published using pmemstream_publish.
 * For regular usage, pmemstream_append should be simpler and safer to use and provide better performance.
 *
 * region_context is an optional parameter which can be obtained from pmemstream_get_region_context.
 * If it's NULL, it will be obtained from its internal structures (which might incur overhead).
 * reserved_entry is updated with offset of the reserved entry.
 * data is updated with a pointer to reserved space - this is a destination for, e.g., custom memcpy. */
int pmemstream_reserve(struct pmemstream *stream, struct pmemstream_region region,
		       struct pmemstream_region_context *region_context, size_t size,
		       struct pmemstream_entry *reserved_entry, void **data);

/* Publish previously custom-written entry.
 * After calling pmemstream_reserve and writing/memcpy'ing data into a reserved_entry, it's required
 * to call this function for setting proper entry's metadata and persist the data.
 *
 * *data has to hold the same data as they were written by user (e.g. in custom memcpy).
 * size of the entry have to match the previous reservation and the actual size of the data written by user. */
int pmemstream_publish(struct pmemstream *stream, struct pmemstream_region region, const void *data, size_t size,
		       struct pmemstream_entry *reserved_entry);

/* Synchronously appends data buffer after last valid entry in region.
 * Fails if no space is available.
 *
 * region_context is an optional parameter which can be obtained from pmemstream_get_region_context.
 * If it's NULL, it will be obtained from its internal structures (which might incur overhead).
 *
 * data is a pointer to data to be appended
 * size is size of the data to be appended
 *
 * new_entry is an optional pointer. On success, it will contain information about position of newly
 * appended entry.
 */
int pmemstream_append(struct pmemstream *stream, struct pmemstream_region region,
		      struct pmemstream_region_context *region_context, const void *data, size_t size,
		      struct pmemstream_entry *new_entry);

// returns pointer to the data of the entry
void *pmemstream_entry_data(struct pmemstream *stream, struct pmemstream_entry entry);

// returns the size of the entry
size_t pmemstream_entry_length(struct pmemstream *stream, struct pmemstream_entry entry);

// an active pmemstream region or entry prevents the truncate function from
// removing its memory location.
// truncation can only affect regions.

int pmemstream_region_iterator_new(struct pmemstream_region_iterator **iterator, const struct pmemstream *stream);

int pmemstream_region_iterator_next(struct pmemstream_region_iterator *iterator, struct pmemstream_region *region);

void pmemstream_region_iterator_delete(struct pmemstream_region_iterator **iterator);

int pmemstream_entry_iterator_new(struct pmemstream_entry_iterator **iterator, const struct pmemstream *stream,
				  struct pmemstream_region region);

// if this function succeeds, entry points to a valid element in the stream, otherwise, it points to a memory
// right after last valid entry or to a beggining of region if there are no valid entries
int pmemstream_entry_iterator_next(struct pmemstream_entry_iterator *iterator, struct pmemstream_region *region,
				   struct pmemstream_entry *entry);

void pmemstream_entry_iterator_delete(struct pmemstream_entry_iterator **iterator);

#ifdef __cplusplus
} /* end extern "C" */
#endif
#endif /* LIBPMEMSTREAM_H */
