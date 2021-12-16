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
	size_t offset;
};

// manages lifecycle of the stream. Can be based on top of a raw pmem2_map
// or a pmemset (TBD).
int pmemstream_from_map(struct pmemstream **stream, size_t block_size, struct pmem2_map *map);
void pmemstream_delete(struct pmemstream **stream);

// stream owns the region object - the user gets a reference, but it's not
// necessary to hold on to it and explicitly delete it.
int pmemstream_region_allocate(struct pmemstream *stream, size_t size, struct pmemstream_region *region);

int pmemstream_region_free(struct pmemstream *stream, struct pmemstream_region region);

size_t pmemstream_region_size(struct pmemstream *stream, struct pmemstream_region region);

int pmemstream_get_region_context(struct pmemstream *stream, struct pmemstream_region region,
				  struct pmemstream_region_context **ctx);

// synchronously appends data buffer to the end of the transaction log space
// fails if no space is available
// 'entry' must provide offset where new entry is appended - it can be obtained from iterator
// after function completes, entry->offset is incremented by count + metadata size
// and new_entry->offset is set to original value of entry->offset
int pmemstream_append(struct pmemstream *stream, struct pmemstream_region *region,
		      struct pmemstream_region_context *region_context, const void *buf, size_t count,
		      struct pmemstream_entry *new_entry);

// returns pointer to the data of the entry
void *pmemstream_entry_data(struct pmemstream *stream, struct pmemstream_entry entry);

// returns the size of the entry
size_t pmemstream_entry_length(struct pmemstream *stream, struct pmemstream_entry entry);

// an active pmemstream region or entry prevents the truncate function from
// removing its memory location.
// truncation can only affect regions.

int pmemstream_region_iterator_new(struct pmemstream_region_iterator **iterator, struct pmemstream *stream);

int pmemstream_region_iterator_next(struct pmemstream_region_iterator *iter, struct pmemstream_region *region);

void pmemstream_region_iterator_delete(struct pmemstream_region_iterator **iterator);

int pmemstream_entry_iterator_new(struct pmemstream_entry_iterator **iterator, struct pmemstream *stream,
				  struct pmemstream_region region);

// if this function succeeds, entry points to a valid element in the stream, otherwise, it points to a memory
// right after last valid entry or to a beggining of region if there are no valid entries
int pmemstream_entry_iterator_next(struct pmemstream_entry_iterator *iter, struct pmemstream_region *region,
				   struct pmemstream_entry *entry);

void pmemstream_entry_iterator_delete(struct pmemstream_entry_iterator **iterator);

#ifdef __cplusplus
} /* end extern "C" */
#endif
#endif /* LIBPMEMSTREAM_H */
