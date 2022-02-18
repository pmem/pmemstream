// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#ifndef LIBPMEMSTREAM_STREAM_HELPERS_HPP
#define LIBPMEMSTREAM_STREAM_HELPERS_HPP

#include <algorithm>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "unittest.hpp"

std::unique_ptr<struct pmemstream, std::function<void(struct pmemstream *)>>
make_pmemstream(const std::string &file, size_t block_size, size_t size, bool truncate = true)
{
	struct pmem2_map *map = map_open(file.c_str(), size, truncate);
	if (map == NULL) {
		throw std::runtime_error(pmem2_errormsg());
	}

	auto map_delete = [](struct pmem2_map *map) { pmem2_map_delete(&map); };
	auto map_sptr = std::shared_ptr<struct pmem2_map>(map, map_delete);

	struct pmemstream *stream;
	int ret = pmemstream_from_map(&stream, block_size, map);
	if (ret == -1) {
		throw std::runtime_error("pmemstream_from_map failed");
	}

	auto stream_delete = [map_sptr](struct pmemstream *stream) { pmemstream_delete(&stream); };
	return std::unique_ptr<struct pmemstream, std::function<void(struct pmemstream *)>>(stream, stream_delete);
}

struct pmemstream_sut {
	pmemstream_sut(const std::string &file, size_t block_size, size_t size, bool truncate = true) : helpers(*this)
	{
		args = std::tuple<std::string, size_t, size_t>(file, block_size, size);
		stream = make_pmemstream(file, block_size, size, truncate);
	};

	pmemstream_sut(const pmemstream_sut &) = delete;
	pmemstream_sut(pmemstream_sut &&rhs) : helpers(*this), args(rhs.args), stream(std::move(rhs.stream))
	{
	}

	/* This function closes and reopens the stream. All pointers to stream data, iterators, etc. are invalidated. */
	void reopen()
	{
		stream.reset();

		/* Add truncate = false flag. */
		auto args = std::tuple_cat(this->args, std::tuple<bool>(false));
		stream = std::apply(make_pmemstream, args);

		helpers.region_runtime.clear();
	}

	std::tuple<int, struct pmemstream_region_runtime *> region_runtime_initialize(struct pmemstream_region region)
	{
		struct pmemstream_region_runtime *runtime;
		int ret = pmemstream_region_runtime_initialize(stream.get(), region, &runtime);
		return {ret, runtime};
	}

	std::tuple<int, struct pmemstream_entry> append(struct pmemstream_region region, const std::string_view &data)
	{
		pmemstream_entry new_entry;
		auto ret = pmemstream_append(stream.get(), region, helpers.region_runtime[region.offset], data.data(),
					     data.size(), &new_entry);
		return {ret, new_entry};
	}

	std::tuple<int, struct pmemstream_entry, void *> reserve(struct pmemstream_region region, size_t size)
	{
		pmemstream_entry reserved_entry;
		void *reserved_data;
		int ret = pmemstream_reserve(stream.get(), region, helpers.region_runtime[region.offset], size,
					     &reserved_entry, &reserved_data);
		return {ret, reserved_entry, reserved_data};
	}

	std::tuple<int, pmemstream_entry> publish(struct pmemstream_region region, void *data, size_t size)
	{
		pmemstream_entry reserved_entry;
		int ret = pmemstream_publish(stream.get(), region, helpers.region_runtime[region.offset], data, size,
					     &reserved_entry);
		return {ret, reserved_entry};
	}

	std::tuple<int, struct pmemstream_region> region_allocate(size_t size)
	{
		pmemstream_region region;
		int ret = pmemstream_region_allocate(stream.get(), size, &region);
		return {ret, region};
	}

	size_t region_size(pmemstream_region region)
	{
		return pmemstream_region_size(stream.get(), region);
	}

	auto entry_iterator(pmemstream_region region)
	{
		struct pmemstream_entry_iterator *eiter;
		UT_ASSERTeq(pmemstream_entry_iterator_new(&eiter, stream.get(), region), 0);

		auto deleter = [](pmemstream_entry_iterator *iter) { pmemstream_entry_iterator_delete(&iter); };
		return std::unique_ptr<struct pmemstream_entry_iterator, decltype(deleter)>(eiter, deleter);
	}

	auto region_iterator()
	{
		struct pmemstream_region_iterator *riter;
		UT_ASSERTeq(pmemstream_region_iterator_new(&riter, stream.get()), 0);

		auto deleter = [](pmemstream_region_iterator *iter) { pmemstream_region_iterator_delete(&iter); };
		return std::unique_ptr<struct pmemstream_region_iterator, decltype(deleter)>(riter, deleter);
	}

	int region_free(pmemstream_region region)
	{
		return pmemstream_region_free(stream.get(), region);
	}

	std::string_view get_entry(struct pmemstream_entry entry)
	{
		auto ptr = reinterpret_cast<const char *>(pmemstream_entry_data(stream.get(), entry));
		return std::string_view(ptr, pmemstream_entry_length(stream.get(), entry));
	}

	/* Implements additional functions, useful for testing. */
	struct helpers_type {
		helpers_type(pmemstream_sut &s) : s(s)
		{
		}

		void append(struct pmemstream_region region, const std::vector<std::string> &data)
		{
			for (const auto &e : data) {
				auto ret = pmemstream_append(s.stream.get(), region, region_runtime[region.offset],
							     e.data(), e.size(), nullptr);
				UT_ASSERTeq(ret, 0);
			}
		}

		void region_runtime_initialize(struct pmemstream_region region)
		{
			struct pmemstream_region_runtime *runtime;
			pmemstream_region_runtime_initialize(s.stream.get(), region, &runtime);
			region_runtime[region.offset] = runtime;
		}

		struct pmemstream_region initialize_single_region(size_t region_size,
								  const std::vector<std::string> &data)
		{
			struct pmemstream_region new_region;
			UT_ASSERTeq(pmemstream_region_allocate(s.stream.get(), region_size, &new_region), 0);
			/* region_size is aligned up to block_size, on allocation, so it may be bigger than expected */
			UT_ASSERT(pmemstream_region_size(s.stream.get(), new_region) >= region_size);

			append(new_region, data);

			return new_region;
		}

		/* Reserve space, write data, and publish (persist) them, within the given region.
		 * Do this for all data in the vector. */
		void reserve_and_publish(struct pmemstream_region region, const std::vector<std::string> &data)
		{
			for (const auto &d : data) {
				/* reserve space for given data */
				struct pmemstream_entry reserved_entry;
				void *reserved_data;
				int ret = pmemstream_reserve(s.stream.get(), region, nullptr, d.size(), &reserved_entry,
							     &reserved_data);
				UT_ASSERTeq(ret, 0);

				/* write into the reserved space and publish (persist) it */
				memcpy(reserved_data, d.data(), d.size());

				ret = pmemstream_publish(s.stream.get(), region, region_runtime[region.offset],
							 d.data(), d.size(), &reserved_entry);
				UT_ASSERTeq(ret, 0);
			}
		}

		struct pmemstream_region get_first_region()
		{
			struct pmemstream_region_iterator *riter;
			int ret = pmemstream_region_iterator_new(&riter, s.stream.get());
			UT_ASSERTne(ret, -1);

			struct pmemstream_region region;
			ret = pmemstream_region_iterator_next(riter, &region);
			UT_ASSERTne(ret, -1);
			pmemstream_region_iterator_delete(&riter);

			return region;
		}

		struct pmemstream_entry get_last_entry(pmemstream_region region)
		{
			struct pmemstream_entry_iterator *eiter;
			UT_ASSERTeq(pmemstream_entry_iterator_new(&eiter, s.stream.get(), region), 0);

			struct pmemstream_entry last_entry = {0};
			struct pmemstream_entry tmp_entry;
			while (pmemstream_entry_iterator_next(eiter, nullptr, &tmp_entry) == 0) {
				last_entry = tmp_entry;
			}

			if (last_entry.offset == 0)
				throw std::runtime_error("No elements in this region.");

			pmemstream_entry_iterator_delete(&eiter);

			return last_entry;
		}

		std::vector<std::string> get_elements_in_region(struct pmemstream_region region)
		{
			std::vector<std::string> result;

			struct pmemstream_entry_iterator *eiter;
			UT_ASSERTeq(pmemstream_entry_iterator_new(&eiter, s.stream.get(), region), 0);

			struct pmemstream_entry entry;
			struct pmemstream_region r;
			while (pmemstream_entry_iterator_next(eiter, &r, &entry) == 0) {
				UT_ASSERTeq(r.offset, region.offset);
				auto data_ptr =
					reinterpret_cast<const char *>(pmemstream_entry_data(s.stream.get(), entry));
				result.emplace_back(data_ptr, pmemstream_entry_length(s.stream.get(), entry));
			}

			pmemstream_entry_iterator_delete(&eiter);

			return result;
		}

		/* XXX: extend to allow more than one extra_data vector */
		void verify(pmemstream_region region, const std::vector<std::string> &data,
			    const std::vector<std::string> &extra_data)
		{
			/* Verify if stream now holds data + extra_data */
			auto all_elements = get_elements_in_region(region);
			auto extra_data_start = all_elements.begin() + static_cast<int>(data.size());

			UT_ASSERT(std::equal(all_elements.begin(), extra_data_start, data.begin()));
			UT_ASSERT(std::equal(extra_data_start, all_elements.end(), extra_data.begin()));
		}

		pmemstream_sut &s;
		std::map<uint64_t, pmemstream_region_runtime *> region_runtime;
	};

	helpers_type helpers;

 private:
	/* Arguments used to create the stream. */
	std::tuple<std::string, size_t, size_t> args;
	std::unique_ptr<struct pmemstream, std::function<void(struct pmemstream *)>> stream;
};

struct pmemstream_with_single_init_region {
	pmemstream_with_single_init_region(const std::vector<std::string> &data)
	    : sut(pmemstream_sut(get_test_config().filename, TEST_DEFAULT_BLOCK_SIZE, TEST_DEFAULT_STREAM_SIZE, true))
	{
		sut.helpers.initialize_single_region(TEST_DEFAULT_REGION_SIZE, data);
	}

	pmemstream_sut sut;
};

struct pmemstream_with_single_empty_region {
	pmemstream_with_single_empty_region()
	    : sut(pmemstream_sut(get_test_config().filename, TEST_DEFAULT_BLOCK_SIZE, TEST_DEFAULT_STREAM_SIZE, true))
	{
		sut.helpers.initialize_single_region(TEST_DEFAULT_REGION_SIZE, {});
	}

	pmemstream_sut sut;
};

struct pmemstream_empty {
	pmemstream_empty()
	    : sut(pmemstream_sut(get_test_config().filename, TEST_DEFAULT_BLOCK_SIZE, TEST_DEFAULT_STREAM_SIZE, true))
	{
	}

	pmemstream_sut sut;
};

#endif /* LIBPMEMSTREAM_STREAM_HELPERS_HPP */
