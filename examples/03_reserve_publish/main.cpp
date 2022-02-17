// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include "examples_helpers.h"
#include "libpmemstream.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include <libpmem2.h>

#define ASSERT(expr)                                                                                                   \
	if (!(expr))                                                                                                   \
		std::cout << __FILE__ << __LINE__ << std::endl;

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

void initialize_stream(const char *path, struct pmem2_map **map, struct pmemstream **stream,
		       struct pmemstream_region *region)
{
	*map = example_map_open(path, EXAMPLE_STREAM_SIZE);
	ASSERT(*map != NULL);

	int ret = pmemstream_from_map(stream, 4096, *map);
	ASSERT(ret == 0);

	ret = pmemstream_region_allocate(*stream, 10240, region);
	ASSERT(ret == 0);
	(void)ret;
}

int verify_stream(pmemstream *stream, pmemstream_region region, data_entry my_entry)
{
	struct pmemstream_entry entry;
	struct pmemstream_entry_iterator *eiter;
	int ret = pmemstream_entry_iterator_new(&eiter, stream, region);
	ASSERT(ret == 0);
	ret = pmemstream_entry_iterator_next(eiter, NULL, &entry);
	ASSERT(ret == 0);
	pmemstream_entry_iterator_delete(&eiter);
	(void)ret;

	auto read_data = reinterpret_cast<const data_entry *>(pmemstream_entry_data(stream, entry));
	data_entry read_entry = *read_data;

	if (read_entry != my_entry) {
		std::cout << "Stored entry " << read_entry.data << " differs from original entry " << my_entry.data
			  << std::endl;
		return -1;
	}

	std::cout << "Hooray, everything works fine" << std::endl;
	return 0;
}

/**
 * This example shows hot to use pmemstream_reserve and pmemstream_publish (with custom write),
 * instead of "the usual" pmemstream_append approach.
 *
 * It creates a file at given path, with size = EXAMPLE_STREAM_SIZE.
 */
int main(int argc, char *argv[])
{
	if (argc < 2) {
		std::cout << "Usage: " << argv[0] << " file" << std::endl;
		return -1;
	}

	/* We initialize stream with a single region */
	struct pmem2_map *map;
	struct pmemstream *stream;
	struct pmemstream_region region;
	initialize_stream(argv[1], &map, &stream, &region);

	data_entry my_entry(42);

	/* Instead of doing regular append (example below), we use reserve and publish functions. */
	// pmemstream_append(stream, region, nullptr, my_entry.data(), my_entry.size(), nullptr);

	struct pmemstream_entry reserved_entry;
	void *reserved_data;
	int ret = pmemstream_reserve(stream, region, nullptr, sizeof(data_entry), &reserved_entry, &reserved_data);
	if (ret != 0) {
		std::cerr << "pmemstream_reserve failed" << std::endl;
		return ret;
	}

	/* After reserving, we write the data as demanded (with the same size as declared in pmemstream_reserve). */

	/* e.g. using memcpy: */
	// memcpy(reserved_data, &my_entry, sizeof(data_entry));

	/* or using placement new: */
	data_entry *emplaced_data = new (reserved_data) data_entry(my_entry);

	/* And we have to publish, what we've written down. */
	ret = pmemstream_publish(stream, region, nullptr, emplaced_data, sizeof(data_entry), &reserved_entry);
	if (ret != 0) {
		std::cerr << "pmemstream_publish failed" << std::endl;
		return ret;
	}

	/* Now, we make sure the only entry is stored as expected. */
	verify_stream(stream, region, my_entry);

	/* Finally we have to clean up */
	pmemstream_region_free(stream, region);
	pmemstream_delete(&stream);
	pmem2_map_delete(&map);

	return 0;
}
