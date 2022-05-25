// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include "examples_helpers.h"
#include "libpmemstream.h"

#include <libpmem2.h>
#include <stdio.h>

struct data_entry {
	uint64_t data;
};

/**
 * This example creates a stream from map2 source, prints its content,
 * and appends monotonically increasing values at the end.
 *
 * It creates a file at given path, with size = EXAMPLE_STREAM_SIZE.
 */
int main(int argc, char *argv[])
{
	if (argc != 2) {
		printf("Usage: %s file\n", argv[0]);
		return -1;
	}

	struct pmem2_map *map = example_map_open(argv[1], EXAMPLE_STREAM_SIZE);
	if (map == NULL) {
		pmem2_perror("pmem2_map");
		return -1;
	}

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

	/* Iterate over all regions. */
	for (pmemstream_region_iterator_seek_first(riter); pmemstream_region_iterator_is_valid(riter) == 0;
	     pmemstream_region_iterator_next(riter)) {
		struct pmemstream_entry entry;
		struct pmemstream_entry_iterator *eiter;

		struct pmemstream_region region = pmemstream_region_iterator_get(riter);

		ret = pmemstream_entry_iterator_new(&eiter, stream, region);
		if (ret == -1) {
			fprintf(stderr, "pmemstream_entry_iterator_new failed\n");
			return ret;
		}

		/* Iterate over all elements in a region and save last entry value. */
		uint64_t last_entry_data;
		while (pmemstream_entry_iterator_next(eiter, NULL, &entry) == 0) {
			const struct data_entry *d = pmemstream_entry_data(stream, entry);
			printf("data entry %lu: %lu in region %lu\n", entry.offset, d->data, region.offset);
			last_entry_data = d->data;
		}
		pmemstream_entry_iterator_delete(&eiter);

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

	/* Allocate new region and append single entry to it. */
	struct pmemstream_region new_region;
	ret = pmemstream_region_allocate(stream, 4096, &new_region);
	if (ret != -1) {
		struct data_entry e;
		e.data = 1;
		struct pmemstream_entry new_entry;

		struct pmemstream_entry_iterator *eiter;
		ret = pmemstream_entry_iterator_new(&eiter, stream, new_region);
		if (ret == -1) {
			fprintf(stderr, "pmemstream_entry_iterator_new failed\n");
			return ret;
		}
		pmemstream_entry_iterator_delete(&eiter);

		ret = pmemstream_append(stream, new_region, NULL, &e, sizeof(e), &new_entry);
		if (ret == -1) {
			fprintf(stderr, "pmemstream_append failed\n");
			return ret;
		}

		const struct data_entry *new_data_entry = pmemstream_entry_data(stream, new_entry);
		printf("new_data_entry: %lu\n", new_data_entry->data);

		e.data++;
		ret = pmemstream_append(stream, new_region, NULL, &e, sizeof(e), &new_entry);
		if (ret == -1) {
			fprintf(stderr, "pmemstream_append failed\n");
			return ret;
		}

		new_data_entry = pmemstream_entry_data(stream, new_entry);
		printf("new_data_entry: %lu\n", new_data_entry->data);
	}

	pmemstream_delete(&stream);
	pmem2_map_delete(&map);

	return 0;
}
