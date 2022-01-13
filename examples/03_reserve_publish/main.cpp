// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include "examples_helpers.h"
#include "libpmemstream.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <libpmem2.h>

struct data_entry {
	uint64_t data;

	data_entry(uint64_t d)
	{
		data = d;
	}

	bool operator!=(const data_entry &other)
	{
		return data != other.data;
	}
};

/**
 * This example shows hot to use pmemstream_reserve and pmemstream_publish (with custom write),
 * instead of "the usual" pmemstream_append approach.
 *
 * It creates a file at given path, with size = EXAMPLE_STREAM_SIZE.
 */
int main(int argc, char *argv[])
{
	if (argc < 2) {
		printf("Usage: %s file\n", argv[0]);
		return -1;
	}

	struct pmem2_map *map = example_map_open(argv[1], EXAMPLE_STREAM_SIZE);
	assert(map != NULL);

	struct pmemstream *stream;
	int ret = pmemstream_from_map(&stream, 4096, map);
	assert(ret == 0);

	struct pmemstream_region region;
	ret = pmemstream_region_allocate(stream, 10240, &region);
	assert(ret == 0);

	data_entry my_entry(42);

	/* Instead of doing regular append (example below), we use reserve and publish functions. */
	// pmemstream_append(stream, region, nullptr, my_entry.data(), my_entry.size(), nullptr);

	struct pmemstream_entry reserved_entry;
	void *reserved_data;
	ret = pmemstream_reserve(stream, region, nullptr, sizeof(data_entry), &reserved_entry, &reserved_data);
	if (ret != 0) {
		fprintf(stderr, "pmemstream_reserve failed\n");
		return ret;
	}

	/* After reserving, we write the data as demanded (with the same size as declared in pmemstream_reserve). */

	/* e.g. using memcpy: */
	// memcpy(reserved_data, &my_entry, sizeof(data_entry));

	/* or using placement new: */
	data_entry *emplaced_data = new (reserved_data) data_entry(my_entry);

	/* And we have to publish, what we've written down. */
	ret = pmemstream_publish(stream, region, emplaced_data, sizeof(data_entry), &reserved_entry);
	if (ret != 0) {
		fprintf(stderr, "pmemstream_publish failed\n");
		return ret;
	}

	/* Now, we make sure the only entry is stored as expected. */
	struct pmemstream_entry entry;
	struct pmemstream_entry_iterator *eiter;
	ret = pmemstream_entry_iterator_new(&eiter, stream, region);
	assert(ret == 0);
	ret = pmemstream_entry_iterator_next(eiter, NULL, &entry);
	assert(ret == 0);
	pmemstream_entry_iterator_delete(&eiter);

	auto read_data = reinterpret_cast<const data_entry *>(pmemstream_entry_data(stream, entry));
	data_entry read_entry = *read_data;

	if (read_entry != my_entry) {
		printf("Stored entry (%lu) differs from original entry (%lu)\n", read_entry.data, my_entry.data);
		return -1;
	} else {
		printf("Hooray, everything works fine\n");
	}

	pmemstream_region_free(stream, region);
	pmemstream_delete(&stream);
	pmem2_map_delete(&map);

	return 0;
}
