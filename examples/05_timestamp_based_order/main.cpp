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

struct timestamped_entry {

	timestamped_entry(size_t timestamp, payload d) : timestamp(timestamp), data(d)
	{
	}

	bool is_older(timestamped_entry other)
	{
		return (timestamp < other.timestamp);
	}

	static constexpr size_t invalid_timestamp = std::numeric_limits<size_t>::max();

	size_t timestamp;
	payload data;
};

struct append_manager {
	append_manager(pmemstream *stream) : stream(stream)
	{
	}

	append_manager(const append_manager &) = delete;
	append_manager &operator=(const append_manager &) = delete;

	void append(struct pmemstream_region region, payload &data)
	{
		std::lock_guard<std::mutex> guard(timestamp_mutex);
		auto to_append = timestamped_entry(timestamp, data);
		timestamp++;

		struct pmemstream_entry new_entry;
		int ret = pmemstream_append(stream, region, NULL, &to_append, sizeof(to_append), &new_entry);
		if (ret == -1) {
			std::runtime_error("pmemstream_append failed\n");
		}
	}

 private:
	pmemstream *stream;
	std::mutex timestamp_mutex;
	size_t timestamp = 0;
};

class entry_iterator {
 public:
	entry_iterator(pmemstream *stream, pmemstream_region &region)
	    : stream(stream), region(region), end(timestamped_entry::invalid_timestamp, payload())
	{
		struct pmemstream_entry_iterator *new_entry_iterator;
		if (pmemstream_entry_iterator_new(&new_entry_iterator, stream, region) != 0) {
			std::runtime_error("Cannot create entry iterators");
		}
		it = std::shared_ptr<pmemstream_entry_iterator>(new_entry_iterator, [](pmemstream_entry_iterator *eit) {
			pmemstream_entry_iterator_delete(&eit);
		});
		++*this;
	}

	void operator++()
	{
		pmemstream_entry entry;
		if (pmemstream_entry_iterator_next(it.get(), &region, &entry) == 0) {
			last_entry = (timestamped_entry *)pmemstream_entry_data(stream, entry);
		} else {
			last_entry = &end;
		}
	}

	bool operator<(entry_iterator &other)
	{
		return (last_entry->is_older(*other.last_entry));
	}

	timestamped_entry get_data()
	{
		return *last_entry;
	}

 private:
	pmemstream *stream;
	pmemstream_region region;
	std::shared_ptr<pmemstream_entry_iterator> it;
	timestamped_entry *last_entry;
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
		printf("Usage: %s file\n", argv[0]);
		return -1;
	}

	std::string path(argv[1]);

	constexpr size_t concurrency = 3;
	constexpr size_t samples_per_thread = 10;

	/* We initialize stream with a single region */
	struct pmem2_map *map;
	struct pmemstream *stream;
	initialize_stream(path, &map, &stream);

	auto regions = create_multiple_regions(&stream, concurrency, EXAMPLE_REGION_SIZE);
	append_manager timestamped_appender(stream);

	parallel_exec(concurrency, [&](size_t thread_id) {
		for (size_t i = 0; i < samples_per_thread; i++) {
			payload entry_identifier(thread_id, i);
			timestamped_appender.append(regions[thread_id], entry_identifier);
		}
	});

	auto entry_iterators = get_entry_iterators(stream, regions);

	for (size_t i = 0; i < concurrency * samples_per_thread; i++) {
		auto oldest_data = std::min_element(entry_iterators.begin(), entry_iterators.end());
		timestamped_entry entry = (*oldest_data).get_data();
		std::cout << "entry timestamp: " << entry.timestamp << " produced by thread: " << entry.data.produced_by
			  << " with index " << entry.data.index << std::endl;
		++(*oldest_data);
	}

	pmemstream_delete(&stream);
	pmem2_map_delete(&map);

	return 0;
}
