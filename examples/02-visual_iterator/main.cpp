// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

#include "libpmemstream.h"

#include <cstdio>
#include <string>
#include <vector>

#include <fcntl.h>
#include <libpmem2.h>
#include <unistd.h>

static struct pmem2_map *map_open(const char *file)
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

	pmem2_config_set_required_store_granularity(config, PMEM2_GRANULARITY_PAGE);

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

using std::string;
using std::vector;

static vector<string> inner_pointers = { "├── ", "│   " };

void print_help(const char* exec_filename) {
	printf("Usage: %s file [--print-as-text]\n", exec_filename);
}
/**
 * This example prints a stream from map2 source.
 *
 * Possible usage:
 * ./example-01-iterate XYZ
 * ./example-02-visual_iterator XYZ
 */
int main(int argc, char *argv[])
{
	bool values_as_text = false;
	struct pmemstream_region region;

	if (argc < 2 || argc > 3) {
		print_help(argv[0]);
		return -1;
	}
	if (argc == 3) {
		if (string(argv[2]) != "--print-as-text") {
			print_help(argv[0]);
			return -1;
		}
		values_as_text = true;
	}

	struct pmem2_map *map = map_open(argv[1]);
	if (map == NULL) {
		pmem2_perror("pmem2_map");
		return -1;
	}

	struct pmemstream *stream;
	int ret = pmemstream_from_map(&stream, 4096, map);
	if (ret != 0) {
		fprintf(stderr, "pmemstream_from_map failed\n");
		return ret;
	}

	struct pmemstream_region_iterator *riter;
	ret = pmemstream_region_iterator_new(&riter, stream);
	if (ret != 0) {
		fprintf(stderr, "pmemstream_region_iterator_new failed\n");
		return ret;
	}

	/* Iterate over all regions. */
	size_t region_id = 0;
	while (pmemstream_region_iterator_next(riter, &region) == 0) {
		struct pmemstream_entry entry;
		struct pmemstream_entry_iterator *eiter;
		ret = pmemstream_entry_iterator_new(&eiter, stream, region);
		if (ret == -1) {
			fprintf(stderr, "pmemstream_entry_iterator_new failed\n");
			return ret;
		}
		printf("%s region%ld: %ld bytes\n", inner_pointers[0].data(), region_id++,
		       pmemstream_region_size(stream, region));

		/* Iterate over all elements in a region and save last entry value. */
		while (pmemstream_entry_iterator_next(eiter, NULL, &entry) == 0) {
			auto entry_length = pmemstream_entry_length(stream, entry);
			printf("%s%s0x%-3X %ldbytes ", inner_pointers[1].data(), inner_pointers[0].data(), (unsigned int)entry.offset, entry_length);

			if (values_as_text) {
				string entry_text(static_cast<char *>(pmemstream_entry_data(stream, entry)), entry_length);
				printf("%s", entry_text.c_str());
			} else {
				auto d = static_cast<unsigned char *>(pmemstream_entry_data(stream, entry));
				for (size_t i = 0; i < entry_length; i++) {
					printf("%.2X ", (int)d[i]);
				}
			}
			printf("\n");
		}
		pmemstream_entry_iterator_delete(&eiter);
	}

	pmemstream_region_iterator_delete(&riter);
	pmemstream_delete(&stream);
	pmem2_map_delete(&map);

	return 0;
}
