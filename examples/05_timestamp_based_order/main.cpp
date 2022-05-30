// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include "examples_helpers.hpp"
#include "libpmemstream.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

#include <libpmem2.h>

/**
 * This example shows how to achieve global ordering of elements concurrently
 * appended to stream. Application operates in the region per thread manner.
 */

/* User data. */
struct payload {
	payload() : produced_by(invalid_value), index(invalid_value)
	{
	}
	payload(size_t produced_by, size_t index) : produced_by(produced_by), index(index)
	{
	}

	static constexpr size_t invalid_value = std::numeric_limits<size_t>::max();

	size_t produced_by;
	size_t index;
};

std::ostream &operator<<(std::ostream &os, const payload &data)
{
	os << " produced by thread " << data.produced_by << " with index " << data.index;
	return os;
}

/* pmememstream entry iterator wrapper, which helps to manage entries from
 * different regions in global order. */
class entry_iterator {
 public:
	entry_iterator(pmemstream *stream, pmemstream_region &region) : stream(stream)
	{
		struct pmemstream_entry_iterator *new_entry_iterator;
		if (pmemstream_entry_iterator_new(&new_entry_iterator, stream, region) != 0) {
			throw std::runtime_error("Cannot create entry iterators");
		}
		it = std::shared_ptr<pmemstream_entry_iterator>(new_entry_iterator, [](pmemstream_entry_iterator *eit) {
			pmemstream_entry_iterator_delete(&eit);
		});
		pmemstream_entry_iterator_seek_first(it.get());
		if (pmemstream_entry_iterator_is_valid(it.get()) != 0) {
			throw std::runtime_error("No entries to iterate");
		}
	}

	void operator++()
	{
		pmemstream_entry_iterator_next(it.get());
	}

	bool operator<(entry_iterator &other)
	{
		if (pmemstream_entry_iterator_is_valid(it.get()) != 0)
			return false;

		if (pmemstream_entry_iterator_is_valid(other.it.get()) != 0)
			return true;

		return get_timestamp() < other.get_timestamp();
	}

	payload get_data()
	{
		if (pmemstream_entry_iterator_is_valid(it.get()) != 0) {
			throw std::runtime_error("Invalid iterator");
		}
		return *reinterpret_cast<const payload *>(
			pmemstream_entry_data(stream, pmemstream_entry_iterator_get(it.get())));
	}

	uint64_t get_timestamp()
	{
		if (pmemstream_entry_iterator_is_valid(it.get()) != 0) {
			throw std::runtime_error("Invalid iterator");
		}

		auto this_entry = pmemstream_entry_iterator_get(it.get());
		return pmemstream_entry_timestamp(stream, this_entry);
	}

 private:
	pmemstream *stream;
	std::shared_ptr<pmemstream_entry_iterator> it;
};

std::vector<entry_iterator> get_entry_iterators(pmemstream *stream, std::vector<pmemstream_region> regions)
{
	std::vector<entry_iterator> entry_iterators;
	for (auto &region : regions) {
		entry_iterators.emplace_back(entry_iterator(stream, region));
	}
	return entry_iterators;
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		std::cerr << "Usage: " << argv[0] << " file" << std::endl;
		return -1;
	}

	std::string path(argv[1]);

	constexpr size_t concurrency = 4;
	constexpr size_t samples_per_thread = 10;

	/* Initialize stream with multiple regions */
	struct pmem2_map *map;
	struct pmemstream *stream;
	initialize_stream(path, &map, &stream);

	auto regions = create_multiple_regions(&stream, concurrency, EXAMPLE_REGION_SIZE);

	/* Concurrently append to many regions with global ordering */
	parallel_exec(concurrency, [&](size_t thread_id) {
		for (size_t i = 0; i < samples_per_thread; i++) {
			payload entry(thread_id, i);
			pmemstream_append(stream, regions[thread_id], NULL, &entry, sizeof(entry), NULL);
		}
	});

	/* Read data in order of appends */
	auto entry_iterators = get_entry_iterators(stream, regions);

	for (size_t i = 0; i < concurrency * samples_per_thread; i++) {
		auto oldest_data = std::min_element(entry_iterators.begin(), entry_iterators.end());
		payload entry = oldest_data->get_data();
		std::cout << entry << " with timestamp: " << oldest_data->get_timestamp() << std::endl;
		++(*oldest_data);
	}

	pmemstream_delete(&stream);
	pmem2_map_delete(&map);

	return 0;
}
