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

int
main(int argc, char *argv[])
{
	if (argc != 2)
		return -1;

	struct pmem2_map *map = map_open(argv[1]);
	if (map == NULL)
		return -1;

	struct pmemstream *stream;
	pmemstream_from_map(&stream, 4096, map);

	struct pmemstream_region_iterator *riter;
	pmemstream_region_iterator_new(&riter, stream);

	struct pmemstream_region region;
	while (pmemstream_region_iterator_next(riter, &region) == 0) {
		struct pmemstream_entry entry;
		struct pmemstream_entry_iterator *eiter;
		pmemstream_entry_iterator_new(&eiter, stream, region);
		uint64_t last_entry_data = 0;
		while (pmemstream_entry_iterator_next(eiter, NULL, &entry) ==
		       0) {
			struct data_entry *d = pmemstream_entry_data(stream, entry);
			printf("data entry %lu: %lu in region %lu\n", entry.offset, d->data, region.offset);
			last_entry_data = d->data;
		}

		pmemstream_entry_iterator_delete(&eiter);
		(void) last_entry_data;
		struct pmemstream_region_context *rcontext;
		pmemstream_region_context_new(&rcontext, stream, region);
		struct pmemstream_tx *tx;
		pmemstream_tx_new(&tx, stream);
		struct data_entry e;
		e.data = last_entry_data + 1;
		struct pmemstream_entry new_entry;
		pmemstream_tx_append(tx, rcontext, &e, sizeof(e), &new_entry);
		pmemstream_tx_commit(&tx);
		pmemstream_region_context_delete(&rcontext);
	}

	pmemstream_region_iterator_delete(&riter);

	struct pmemstream_tx *tx;
	pmemstream_tx_new(&tx, stream);

	struct pmemstream_region new_region;
	pmemstream_tx_region_allocate(tx, stream, 4096, &new_region);

	struct pmemstream_region_context *rcontext;
	pmemstream_region_context_new(&rcontext, stream, new_region);

	struct data_entry e;
	e.data = 1;
	struct pmemstream_entry new_entry;
	pmemstream_tx_append(tx, rcontext, &e, sizeof(e), &new_entry);
	pmemstream_tx_commit(&tx);

	struct data_entry *new_data_entry = pmemstream_entry_data(stream, new_entry);
	printf("new_data_entry: %lu\n", new_data_entry->data);

	pmemstream_region_context_delete(&rcontext);

	pmemstream_delete(&stream);

	pmem2_map_delete(&map);

	return 0;
}
