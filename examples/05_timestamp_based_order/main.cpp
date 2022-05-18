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

/* Class which holds user data and timestamp. */
struct timestamped_entry {

	timestamped_entry(size_t timestamp, payload d) : timestamp(timestamp), data(d)
	{
	}

	bool is_older(timestamped_entry *other)
	{
		return (timestamp < other->timestamp);
	}

	static constexpr size_t invalid_timestamp = std::numeric_limits<size_t>::max();

	size_t timestamp;
	payload data;
};

std::ostream &operator<<(std::ostream &os, const timestamped_entry &entry)
{
	os << "entry timestamp " << entry.timestamp << " with data (" << entry.data << ")";
	return os;
}

/* Wrapper class for pmemstream_append, which guarantee that appends from
 * multiple threads occur in order of monotonically increasing timestamps. */
struct append_manager {
	append_manager(pmemstream *stream) : stream(stream)
	{
	}

	append_manager(const append_manager &) = delete;
	append_manager &operator=(const append_manager &) = delete;

	void append(struct pmemstream_region region, payload &data)
	{
		/* Acquire lock to simulate atomicity of append with timestamp. */
		std::lock_guard<std::mutex> guard(timestamp_mutex);
		auto to_append = timestamped_entry(timestamp, data);
		timestamp++;

		int ret = pmemstream_append(stream, region, NULL, &to_append, sizeof(to_append), NULL);
		if (ret != 0) {
			std::runtime_error("pmemstream_append failed\n");
		}
	}

 private:
	pmemstream *stream;
	std::mutex timestamp_mutex;
	size_t timestamp = 0;
};

/* pmememstream entry iterator wrapper, which helps to manage entries from
 * different regions in global order. */
class entry_iterator {
 public:
	entry_iterator(pmemstream *stream, pmemstream_region &region)
	    : stream(stream), region(region), end(timestamped_entry::invalid_timestamp, payload())
	{
		struct pmemstream_entry_iterator *new_entry_iterator;
		if (pmemstream_entry_iterator_new(&new_entry_iterator, stream, region) != 0) {
			throw std::runtime_error("Cannot create entry iterators");
		}
		it = std::shared_ptr<pmemstream_entry_iterator>(new_entry_iterator, [](pmemstream_entry_iterator *eit) {
			pmemstream_entry_iterator_delete(&eit);
		});
		pmemstream_entry_iterator_seek_first(it.get());
		if(pmemstream_entry_iterator_is_valid(it.get()) != 0){
			throw std::runtime_error("No entries to iterate");
		}
	}

	void operator++()
	{
		pmemstream_entry_iterator_next(it.get());
	}

	bool operator<(entry_iterator &other)
	{
		pmemstream_entry entry = pmemstream_entry_iterator_get(it.get());
		timestamped_entry *ts_entry = (timestamped_entry*)&entry;
		auto other_entry = pmemstream_entry_iterator_get(other.it.get());
		timestamped_entry *ts_o_entry = (timestamped_entry*)&other_entry;
		return (ts_entry->is_older(ts_o_entry));
	}

	timestamped_entry get_data()
	{
		if(pmemstream_entry_iterator_is_valid(it.get()) == 0) {
			pmemstream_entry entry = pmemstream_entry_iterator_get(it.get());
			return *(timestamped_entry *)pmemstream_entry_data(stream, entry);
		}
		return end;
	}

 private:
	pmemstream *stream;
	pmemstream_region region;
	std::shared_ptr<pmemstream_entry_iterator> it;
	timestamped_entry end;
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

	constexpr size_t concurrency = 2;
	constexpr size_t samples_per_thread = 10;

	/* Initialize stream with multiple regions */
	struct pmem2_map *map;
	struct pmemstream *stream;
	initialize_stream(path, &map, &stream);

	auto regions = create_multiple_regions(&stream, concurrency, EXAMPLE_REGION_SIZE);
	append_manager timestamped_appender(stream);

	/* Concurrently append to many regions with global ordering */
	parallel_exec(concurrency, [&](size_t thread_id) {
		for (size_t i = 0; i < samples_per_thread; i++) {
			payload entry_identifier(thread_id, i);
			timestamped_appender.append(regions[thread_id], entry_identifier);
		}
	});

	/* Read data in order of appends */
	auto entry_iterators = get_entry_iterators(stream, regions);

	for (size_t i = 0; i < concurrency * samples_per_thread; i++) {
		auto oldest_data = std::min_element(entry_iterators.begin(), entry_iterators.end());
		timestamped_entry entry = oldest_data->get_data();
		std::cout << entry << std::endl;
		++(*oldest_data);
	}

	pmemstream_delete(&stream);
	pmem2_map_delete(&map);

	return 0;
}
