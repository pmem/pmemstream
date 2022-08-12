// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include "examples_helpers.h"
#include "libpmemstream.h"

#include <libpmem2.h>
#include <stdio.h>

/* A simple wrapper for integer value - an example, arbitrary stream entry. */
struct data_entry {
	uint64_t data;
};

/**
 * This example creates a stream from map2 source, prints its content,
 * and appends monotonically increasing values at the end.
 *
 * It creates a file at given path, with size = EXAMPLE_STREAM_SIZE.
 *
 * Usage:
 * ./example-01_basic_iterate file
 *
 * When calling this example multiple time (with the same file) it will successively add more entries.
 */
int main(int argc, char *argv[])
{
	if (argc != 2) {
		printf("Usage: %s file\n", argv[0]);
		return -1;
	}

	/* Use our examples' helper function to create a pmem2 mapping for given file. */
	struct pmem2_map *map = example_map_open(argv[1], EXAMPLE_STREAM_SIZE);
	if (map == NULL) {
		pmem2_perror("pmem2_map");
		return -1;
	}

	/* Open/create a pmemstream instance (here, from pmem2_map with block_size = 4096). */
	struct pmemstream *stream;
	int ret = pmemstream_from_map(&stream, 4096, map);
	if (ret == -1) {
		fprintf(stderr, "pmemstream_from_map failed\n");
		return ret;
	}

	struct pmemstream_region_iterator *riter;
	ret = pmemstream_region_iterator_new(&riter, stream);
	if (ret == -1) {
		fprintf(stderr, "pmemstream_region_iterator_new failed\n");
		return ret;
	}

	/* Iterate over all existing regions and append a new entry. */
	for (pmemstream_region_iterator_seek_first(riter); pmemstream_region_iterator_is_valid(riter) == 0;
	     pmemstream_region_iterator_next(riter)) {
		struct pmemstream_entry_iterator *eiter;

		struct pmemstream_region region = pmemstream_region_iterator_get(riter);

		ret = pmemstream_entry_iterator_new(&eiter, stream, region);
		if (ret == -1) {
			fprintf(stderr, "pmemstream_entry_iterator_new failed\n");
			return ret;
		}

		/* Iterate over all elements in a region and save last entry value. */
		uint64_t last_entry_data = 0;
		for (pmemstream_entry_iterator_seek_first(eiter); pmemstream_entry_iterator_is_valid(eiter) == 0;
		     pmemstream_entry_iterator_next(eiter)) {
			struct pmemstream_entry entry = pmemstream_entry_iterator_get(eiter);
			const struct data_entry *d = pmemstream_entry_data(stream, entry);
			if (d == NULL) {
				fprintf(stderr, "pmemstream_entry_data failed\n");
				return -1;
			}
			printf("data entry (at offset: %lu) has value: %lu and it's located in region at offset: %lu\n",
			       entry.offset, d->data, region.offset);
			last_entry_data = d->data;
		}

		/* Free resources when they are not needed anymore. */
		pmemstream_entry_iterator_delete(&eiter);

		/* Append an entry (with value one bigger than the last entry in the region). */
		struct data_entry e;
		e.data = last_entry_data + 1;
		struct pmemstream_entry new_entry;
		ret = pmemstream_append(stream, region, NULL, &e, sizeof(e), &new_entry);
		if (ret == -1) {
			fprintf(stderr, "pmemstream_append failed\n");
			return ret;
		}
	}
	pmemstream_region_iterator_delete(&riter);

	/* Allocate new region (here of size = 4096) and append a single entry to it. */
	struct pmemstream_region new_region;
	ret = pmemstream_region_allocate(stream, 4096, &new_region);
	if (ret != -1) {
		struct data_entry e;
		e.data = 1;
		struct pmemstream_entry new_entry;

		ret = pmemstream_append(stream, new_region, NULL, &e, sizeof(e), &new_entry);
		if (ret == -1) {
			fprintf(stderr, "pmemstream_append failed\n");
			return ret;
		}

		/* After appending new entry we read it back to see if it's correct. */
		const struct data_entry *read_data = pmemstream_entry_data(stream, new_entry);
		printf("We've successfully added new entry and it's data is: %lu\n", read_data->data);
	} else {
		/* Allocation of new region may fail due to insufficient space in pmemstream
		 * or wrong size (or, in general, improper parameter) passed to the function. */
		printf("pmemstream_region_allocate failed\n");
	}

	/* Clean up at the end - properly close stream and mapping. */
	pmemstream_delete(&stream);
	pmem2_map_delete(&map);

	return 0;
}
