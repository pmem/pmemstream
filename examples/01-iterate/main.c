// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

#include "libpmemstream.h"
#include <assert.h>
#include <fcntl.h>
#include <libpmem2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static struct pmem2_map *
map_open(const char *file)
{
	struct pmem2_source *source;
	struct pmem2_config *config;
	struct pmem2_map *map = NULL;

	int fd = open(file, O_RDWR);
	if (fd < 0)
		return NULL;

	if (pmem2_source_from_fd(&source, fd) != 0)
		goto err_fd;

	if (pmem2_config_new(&config) != 0)
		goto err_config;

	pmem2_config_set_required_store_granularity(config,
						    PMEM2_GRANULARITY_PAGE);

	if (pmem2_map_new(&map, config, source) != 0)
		goto err_map;

err_map:
	pmem2_config_delete(&config);
err_config:
	pmem2_source_delete(&source);
err_fd:
	close(fd);

	return map;
}

struct data_entry {
	uint64_t data;
};

/**
 * This example creates a stream from map2 source, prints it's content and appends monotonically
 * increasing values at the end.
 *
 * It accepts a path to already existing, zeroed out file.
 * (File can be create for example by dd: dd if=/dev/zero of=file bs=1024 count=1024)
 */
int
main(int argc, char *argv[])
{
	if (argc != 2) {
		printf("Usage: %s file\n", argv[0]);
		return -1;
	}

	struct pmem2_map *map = map_open(argv[1]);
	if (map == NULL) {
		pmem2_perror("pmem2_map");
		return -1;
	}

	struct pmemstream *stream;
	int ret = pmemstream_from_map(&stream, 4096, map);
	if (ret == -1) {
		fprintf(stderr, "pmemstream_from_map failed\n");
		return -1;
	}

	struct pmemstream_region_iterator *riter;
	ret = pmemstream_region_iterator_new(&riter, stream);
	if (ret == -1) {
		fprintf(stderr, "pmemstream_region_iterator_new failed\n");
		return -1;
	}

	struct pmemstream_region region;

	/* Iterate over all regions. */
	while (pmemstream_region_iterator_next(riter, &region) == 0) {
		struct pmemstream_entry entry;
		struct pmemstream_entry_iterator *eiter;
		ret = pmemstream_entry_iterator_new(&eiter, stream, region);
		if (ret == -1) {
			fprintf(stderr, "pmemstream_entry_iterator_new failed\n");
			return -1;
		}

		/* Iterate over all elements in a region and save last entry value. */
		uint64_t last_entry_data;
		while (pmemstream_entry_iterator_next(eiter, NULL, &entry) == 0) {
			struct data_entry *d = pmemstream_entry_data(stream, entry);
			printf("data entry %lu: %lu in region %lu\n", entry.offset, d->data, region.offset);
			last_entry_data = d->data;
		}
		pmemstream_entry_iterator_delete(&eiter);

		/* Create region context and use it to append new value at the end. */
		struct pmemstream_region_context *rcontext;
		ret = pmemstream_region_context_new(&rcontext, stream, region);
		if (ret == -1) {
			fprintf(stderr, "pmemstream_region_context_new failed\n");
			return -1;
		}

		struct data_entry e;
		e.data = last_entry_data + 1;
		struct pmemstream_entry new_entry;
		ret = pmemstream_append(stream, rcontext, &e, sizeof(e), &new_entry);
		if (ret == -1) {
			fprintf(stderr, "pmemstream_append failed\n");
			return -1;
		}

		pmemstream_region_context_delete(&rcontext);
	}

	pmemstream_region_iterator_delete(&riter);

	/* Allocate new region and append single entry to it. */
	struct pmemstream_region new_region;
	ret = pmemstream_region_allocate(stream, 4096, &new_region);
	if (ret != -1) {
		struct pmemstream_region_context *rcontext;
		ret = pmemstream_region_context_new(&rcontext, stream, new_region);
		if (ret == -1) {
			fprintf(stderr, "pmemstream_region_context_new failed\n");
			return -1;
		}

		struct data_entry e;
		e.data = 1;
		struct pmemstream_entry new_entry;
		ret = pmemstream_append(stream, rcontext, &e, sizeof(e), &new_entry);
		if (ret == -1) {
			fprintf(stderr, "pmemstream_append failed\n");
			return -1;
		}

		struct data_entry *new_data_entry = pmemstream_entry_data(stream, new_entry);
		printf("new_data_entry: %lu\n", new_data_entry->data);

		pmemstream_region_context_delete(&rcontext);
	}

	pmemstream_delete(&stream);

	pmem2_map_delete(&map);

	return 0;
}
