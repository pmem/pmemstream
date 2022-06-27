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

#include "stream_span_helpers.hpp"
#include "unittest.hpp"

static inline bool operator==(const struct pmemstream_region lhs, const struct pmemstream_region rhs)
{
	return lhs.offset == rhs.offset;
}

static inline std::unique_ptr<struct pmemstream, std::function<void(struct pmemstream *)>>
make_pmemstream(const std::string &file, size_t block_size, size_t size, bool truncate = true)
{
	struct pmem2_map *map = map_open(file.c_str(), size, truncate);
	if (map == NULL) {
		throw std::runtime_error(pmem2_errormsg());
	}

	auto map_sptr = std::shared_ptr<struct pmem2_map>(map, map_delete);

	struct pmemstream *stream;
	int ret = pmemstream_from_map(&stream, block_size, map);
	if (ret == -1) {
		throw std::runtime_error("pmemstream_from_map failed");
	}

	auto stream_delete = [map_sptr](struct pmemstream *stream) { pmemstream_delete(&stream); };
	return std::unique_ptr<struct pmemstream, std::function<void(struct pmemstream *)>>(stream, stream_delete);
}

/* pmem functions' mocks */
static inline void *memcpy_mock(void *dest, const void *src, size_t len, unsigned flags)
{
	return NULL;
}

static inline void *memset_mock(void *dest, int c, size_t len, unsigned flags)
{
	return NULL;
}

static inline void flush_mock(const void *ptr, size_t size)
{
	return;
}

static inline void persist_mock(const void *ptr, size_t size)
{
	return;
}

static inline void drain_mock(void)
{
	return;
}
/* pmem functions' mocks */

namespace pmem
{

struct stream {
	stream(const std::string &file, size_t block_size, size_t size, bool truncate = true)
	    : c_stream(make_pmemstream(file, block_size, size, truncate))
	{
	}

	stream(const stream &) = delete;
	stream(stream &&) = default;
	stream &operator=(const stream &) = delete;
	stream &operator=(stream &&) = default;

	pmemstream *c_ptr()
	{
		return c_stream.get();
	}

	const pmemstream *c_ptr() const
	{
		return c_stream.get();
	}

	void close()
	{
		c_stream.reset();
	}

	std::tuple<int, struct pmemstream_region_runtime *> region_runtime_initialize(struct pmemstream_region region)
	{
		struct pmemstream_region_runtime *runtime = nullptr;
		int ret = pmemstream_region_runtime_initialize(c_stream.get(), region, &runtime);
		return {ret, runtime};
	}

	std::tuple<int, struct pmemstream_entry> append(struct pmemstream_region region, const std::string_view &data,
							pmemstream_region_runtime *region_runtime = nullptr)
	{
		pmemstream_entry new_entry = {0};
		auto ret =
			pmemstream_append(c_stream.get(), region, region_runtime, data.data(), data.size(), &new_entry);
		return {ret, new_entry};
	}

	std::tuple<int, struct pmemstream_entry> async_append(struct vdm *vdm, struct pmemstream_region region,
							      const std::string_view &data,
							      pmemstream_region_runtime *region_runtime = nullptr)
	{
		struct pmemstream_entry entry;
		auto ret = pmemstream_async_append(c_stream.get(), vdm, region, region_runtime, data.data(),
						   data.size(), &entry);
		return {ret, entry};
	}

	std::tuple<int, struct pmemstream_entry, void *> reserve(struct pmemstream_region region, size_t size,
								 pmemstream_region_runtime *region_runtime = nullptr)
	{
		pmemstream_entry reserved_entry = {0};
		void *reserved_data = nullptr;
		int ret = pmemstream_reserve(c_stream.get(), region, region_runtime, size, &reserved_entry,
					     &reserved_data);
		return {ret, reserved_entry, reserved_data};
	}

	int publish(struct pmemstream_region region, const void *, size_t size, struct pmemstream_entry reserved_entry,
		    pmemstream_region_runtime *region_runtime = nullptr)
	{
		// XXX: remove data from API
		return pmemstream_publish(c_stream.get(), region, region_runtime, reserved_entry, size);
	}

	auto async_publish(struct pmemstream_region region, const void *, size_t size,
			   struct pmemstream_entry reserved_entry, pmemstream_region_runtime *region_runtime = nullptr)
	{
		// XXX: remove data from API
		return pmemstream_async_publish(c_stream.get(), region, region_runtime, reserved_entry, size);
	}

	std::tuple<int, struct pmemstream_region> region_allocate(size_t size)
	{
		pmemstream_region region = {0};
		int ret = pmemstream_region_allocate(c_stream.get(), size, &region);
		return {ret, region};
	}

	size_t region_size(pmemstream_region region)
	{
		return pmemstream_region_size(c_stream.get(), region);
	}

	size_t region_usable_size(pmemstream_region region)
	{
		return pmemstream_region_usable_size(c_stream.get(), region);
	}

	auto entry_timestamp(pmemstream_entry entry)
	{
		return pmemstream_entry_timestamp(c_stream.get(), entry);
	}

	auto entry_iterator(pmemstream_region region)
	{
		struct pmemstream_entry_iterator *eiter;
		int ret = pmemstream_entry_iterator_new(&eiter, c_stream.get(), region);
		if (ret != 0) {
			throw std::runtime_error("pmemstream_entry_iterator_new failed");
		}

		auto deleter = [](pmemstream_entry_iterator *iter) { pmemstream_entry_iterator_delete(&iter); };
		return std::unique_ptr<struct pmemstream_entry_iterator, decltype(deleter)>(eiter, deleter);
	}

	auto region_iterator()
	{
		struct pmemstream_region_iterator *riter;
		int ret = pmemstream_region_iterator_new(&riter, c_stream.get());
		if (ret != 0) {
			throw std::runtime_error("pmemstream_region_iterator_new failed");
		}

		auto deleter = [](pmemstream_region_iterator *iter) { pmemstream_region_iterator_delete(&iter); };
		return std::unique_ptr<struct pmemstream_region_iterator, decltype(deleter)>(riter, deleter);
	}

	int region_free(pmemstream_region region)
	{
		return pmemstream_region_free(c_stream.get(), region);
	}

	std::string_view get_entry(struct pmemstream_entry entry)
	{
		auto ptr = reinterpret_cast<const char *>(pmemstream_entry_data(c_stream.get(), entry));
		return std::string_view(ptr, pmemstream_entry_length(c_stream.get(), entry));
	}

	auto committed_timestamp()
	{
		return pmemstream_committed_timestamp(c_stream.get());
	}

	auto persisted_timestamp()
	{
		return pmemstream_persisted_timestamp(c_stream.get());
	}

	auto async_wait_persisted(uint64_t timestamp)
	{
		return pmemstream_async_wait_persisted(c_stream.get(), timestamp);
	}

	auto async_wait_committed(uint64_t timestamp)
	{
		return pmemstream_async_wait_committed(c_stream.get(), timestamp);
	}

 private:
	std::unique_ptr<struct pmemstream, std::function<void(struct pmemstream *)>> c_stream;
}; /* struct stream */

} // namespace pmem

template <typename FutureT>
struct future_wrapper {
	future_wrapper(FutureT &&future) : future(std::make_unique<FutureT>(std::move(future)))
	{
	}

	future_wrapper() : future(std::make_unique<FutureT>())
	{
		FUTURE_INIT_COMPLETE(future.get());
	}

	future_wrapper(future_wrapper &&) = default;
	future_wrapper &operator=(future_wrapper &&) = default;

	future_wrapper(const future_wrapper &) = delete;
	future_wrapper &operator=(const future_wrapper &) = delete;

	auto poll()
	{
		return future_poll(FUTURE_AS_RUNNABLE(future.get()), NULL);
	}

	~future_wrapper()
	{
		if (!future)
			return;

		/* poll until persisted */
		while (poll() != FUTURE_STATE_COMPLETE)
			;
	}

	std::unique_ptr<FutureT> future;
};

/* Implements additional functions, useful for testing. */
struct pmemstream_helpers_type {
	using thread_data_mover_type = std::unique_ptr<struct data_mover_threads, decltype(&data_mover_threads_delete)>;

	pmemstream_helpers_type(pmem::stream &stream, bool call_region_runtime_initialize)
	    : stream(stream),
	      region_runtime(),
	      call_region_runtime_initialize(call_region_runtime_initialize),
	      thread_mover_handle(thread_data_mover_type(data_mover_threads_default(), &data_mover_threads_delete))
	{
	}

	pmemstream_helpers_type(const pmemstream_helpers_type &) = delete;
	pmemstream_helpers_type(pmemstream_helpers_type &&) = delete;
	pmemstream_helpers_type &operator=(const pmemstream_helpers_type &) = delete;
	pmemstream_helpers_type &operator=(pmemstream_helpers_type &&) = delete;

	void append(struct pmemstream_region region, const std::vector<std::string> &data)
	{
		for (const auto &e : data) {
			pmemstream_region_runtime *rrt = nullptr;
			auto it = region_runtime.find(region.offset);
			if (it != region_runtime.end()) {
				rrt = it->second;
			}

			auto [ret, entry] = stream.append(region, e, rrt);
			UT_ASSERTeq(ret, 0);
		}
	}

	future_wrapper<pmemstream_async_wait_fut> async_append(struct pmemstream_region region,
							       const std::vector<std::string> &data)
	{
		struct vdm *thread_mover = data_mover_threads_get_vdm(thread_mover_handle.get());

		struct pmemstream_entry entry;
		for (size_t i = 0; i < data.size(); ++i) {
			auto [ret, new_entry] =
				stream.async_append(thread_mover, region, data[i], region_runtime[region.offset]);
			UT_ASSERTeq(ret, 0);
			entry = new_entry;
		}

		if (data.size()) {
			auto future = stream.async_wait_persisted(stream.entry_timestamp(entry));
			return future_wrapper<pmemstream_async_wait_fut>(std::move(future));
		} else {
			return future_wrapper<pmemstream_async_wait_fut>();
		}
	}

	struct pmemstream_region initialize_single_region(size_t region_size, const std::vector<std::string> &data)
	{
		auto [ret, region] = stream.region_allocate(region_size);
		UT_ASSERTeq(ret, 0);
		/* region_size is aligned up to block_size, on allocation, so it may be bigger than expected */
		UT_ASSERT(stream.region_size(region) >= region_size);

		if (call_region_runtime_initialize) {
			auto [ret, runtime] = stream.region_runtime_initialize(region);
			region_runtime[region.offset] = runtime;
		}

		append(region, data);

		return region;
	}

	/* allocate max number of regions (with the same size and the same initial data) */
	auto initialize_multi_regions(size_t max_regions_count, size_t region_size,
				      const std::vector<std::string> &data)
	{
		std::vector<struct pmemstream_region> regions;

		for (size_t i = 0; i < max_regions_count; ++i) {
			auto [ret, region] = stream.region_allocate(region_size);
			if (ret != 0) {
				break;
			}

			/* region_size is aligned up to block_size, on allocation, so it may be bigger than expected */
			UT_ASSERT(stream.region_size(region) >= region_size);

			if (call_region_runtime_initialize) {
				auto [ret, runtime] = stream.region_runtime_initialize(region);
				region_runtime[region.offset] = runtime;
			}

			append(region, data);
			regions.push_back(region);
		}

		UT_ASSERT(regions.size() > 0 && regions.size() <= max_regions_count);
		return regions;
	}

	/* Reserve space, write data, and publish (persist) them, within the given region.
	 * Do this for all data in the vector. */
	void reserve_and_publish(struct pmemstream_region region, const std::vector<std::string> &data)
	{
		for (const auto &d : data) {
			/* reserve space for given data */
			auto [ret, reserved_entry, reserved_data] =
				stream.reserve(region, d.size(), region_runtime[region.offset]);
			UT_ASSERTeq(ret, 0);

			/* write into the reserved space and publish (persist) it */
			memcpy(reserved_data, d.data(), d.size());

			auto ret_p = stream.publish(region, reinterpret_cast<const void *>(d.data()), d.size(),
						    reserved_entry, region_runtime[region.offset]);
			UT_ASSERTeq(ret_p, 0);
		}
	}

	std::vector<struct pmemstream_region> allocate_regions(size_t n, size_t region_size)
	{
		std::vector<struct pmemstream_region> regions;

		for (size_t i = 0; i < n; i++) {
			auto [ret, region] = stream.region_allocate(region_size);
			UT_ASSERTeq(ret, 0);

			/* region_size is aligned up to block_size, on allocation, so it may be bigger than expected */
			UT_ASSERT(stream.region_size(region) >= region_size);

			regions.push_back(region);
		}

		return regions;
	}

	/* Get n-th region in the stream (counts from 0);
	 * It will fail assertion if n-th region is missing. */
	struct pmemstream_region get_region(size_t n)
	{
		int ret = 0;
		size_t counter = 0;
		struct pmemstream_region region;

		auto riter = stream.region_iterator();
		pmemstream_region_iterator_seek_first(riter.get());

		do {
			ret = pmemstream_region_iterator_is_valid(riter.get());
			UT_ASSERTeq(ret, 0);
			region = pmemstream_region_iterator_get(riter.get());
			pmemstream_region_iterator_next(riter.get());
		} while (counter++ < n);

		return region;
	}

	struct pmemstream_region get_first_region()
	{
		return get_region(0);
	}

	/* Get all regions in the stream */
	std::vector<struct pmemstream_region> get_regions()
	{
		auto riter = stream.region_iterator();
		pmemstream_region_iterator_seek_first(riter.get());
		std::vector<struct pmemstream_region> regions;
		while (pmemstream_region_iterator_is_valid(riter.get()) == 0) {
			struct pmemstream_region region = pmemstream_region_iterator_get(riter.get());

			regions.push_back(region);
			pmemstream_region_iterator_next(riter.get());
		}

		return regions;
	}

	struct pmemstream_entry get_last_entry(pmemstream_region region)
	{
		auto eiter = stream.entry_iterator(region);

		struct pmemstream_entry last_entry = {0};
		for (pmemstream_entry_iterator_seek_first(eiter.get());
		     pmemstream_entry_iterator_is_valid(eiter.get()) == 0;
		     pmemstream_entry_iterator_next(eiter.get())) {
			last_entry = pmemstream_entry_iterator_get(eiter.get());
		}

		if (last_entry.offset == 0)
			throw std::runtime_error("No elements in this region.");

		return last_entry;
	}

	std::vector<std::string> get_elements_in_region(struct pmemstream_region region, bool persisted_only = true)
	{
		std::vector<std::string> result;

		auto eiter = stream.entry_iterator(region);

		for (pmemstream_entry_iterator_seek_first(eiter.get());
		     pmemstream_entry_iterator_is_valid(eiter.get()) == 0;
		     pmemstream_entry_iterator_next(eiter.get())) {
			auto entry = pmemstream_entry_iterator_get(eiter.get());

			if (persisted_only && stream.entry_timestamp(entry) > stream.persisted_timestamp())
				break;

			result.emplace_back(stream.get_entry(entry));
		}

		return result;
	}

	size_t count_regions()
	{
		auto riter = stream.region_iterator();

		size_t region_counter = 0;
		pmemstream_region_iterator_seek_first(riter.get());
		while (pmemstream_region_iterator_is_valid(riter.get()) == 0) {
			++region_counter;
			pmemstream_region_iterator_next(riter.get());
		}
		return region_counter;
	}

	/* Removes region with given offset.
	 * It will fail assertion if such region is missing. */
	int remove_region(size_t offset)
	{
		int ret = 0;
		struct pmemstream_region region;
		auto riter = stream.region_iterator();
		pmemstream_region_iterator_seek_first(riter.get());

		do {
			ret = pmemstream_region_iterator_is_valid(riter.get());
			UT_ASSERTeq(ret, 0);
			region = pmemstream_region_iterator_get(riter.get());
			pmemstream_region_iterator_next(riter.get());
		} while (region.offset != offset);

		return stream.region_free(region);
	}

	/* Removes given regions. */
	void remove_regions(std::vector<struct pmemstream_region> regions)
	{
		for (auto r : regions) {
			UT_ASSERTeq(remove_region(r.offset), 0);
		}
	}

	/* XXX: extend to allow more than one extra_data vector */
	void verify(pmemstream_region region, const std::vector<std::string> &data,
		    const std::vector<std::string> &extra_data, bool persisted = true)
	{
		/* Verify if stream now holds data + extra_data */
		auto all_elements = get_elements_in_region(region, persisted);
		auto extra_data_start = all_elements.begin() + static_cast<int>(data.size());

		UT_ASSERTeq(all_elements.size(), data.size() + extra_data.size());
		UT_ASSERT(std::equal(all_elements.begin(), extra_data_start, data.begin()));
		UT_ASSERT(std::equal(extra_data_start, all_elements.end(), extra_data.begin()));
	}

	pmem::stream &stream;
	std::map<uint64_t, pmemstream_region_runtime *> region_runtime;
	bool call_region_runtime_initialize = false;
	thread_data_mover_type thread_mover_handle;
}; /* struct pmemstream_helpers_type */

struct pmemstream_test_base {
	pmemstream_test_base(const std::string &file, size_t block_size, size_t size, bool truncate = true,
			     bool call_initialize_region_runtime = false,
			     bool call_initialize_region_runtime_after_reopen = false)
	    : sut(file, block_size, size, truncate),
	      file(file),
	      block_size(block_size),
	      size(size),
	      helpers(sut, call_initialize_region_runtime),
	      call_initialize_region_runtime(call_initialize_region_runtime),
	      call_initialize_region_runtime_after_reopen(call_initialize_region_runtime_after_reopen)
	{
	}

	pmemstream_test_base(const pmemstream_test_base &) = delete;
	pmemstream_test_base &operator=(const pmemstream_test_base &) = delete;

	pmemstream_test_base(pmemstream_test_base &&rhs)
	    : sut(std::move(rhs.sut)),
	      file(rhs.file),
	      block_size(rhs.block_size),
	      size(rhs.size),
	      helpers(sut, rhs.call_initialize_region_runtime),
	      call_initialize_region_runtime(rhs.call_initialize_region_runtime),
	      call_initialize_region_runtime_after_reopen(rhs.call_initialize_region_runtime_after_reopen)
	{
	}

	/* This function closes and reopens the stream. All pointers to stream data, iterators, etc. are invalidated. */
	void reopen()
	{
		sut.close();

		sut = pmem::stream(file, block_size, size, false);

		if (call_initialize_region_runtime_after_reopen) {
			for (auto &r : helpers.region_runtime) {
				auto [ret, runtime] = sut.region_runtime_initialize(pmemstream_region{r.first});
				UT_ASSERTeq(ret, 0);
				r.second = runtime;
			}
		} else {
			/* Invalidate. */
			helpers.region_runtime.clear();
		}
	}

	/* Stream under test. */
	pmem::stream sut;

	/* Arguments used to create the stream. */
	std::string file;
	size_t block_size;
	size_t size;

	pmemstream_helpers_type helpers;

	bool call_initialize_region_runtime = false;
	bool call_initialize_region_runtime_after_reopen = false;
}; /* struct pmemstream_test_base */

static inline std::ostream &operator<<(std::ostream &os, const pmemstream_test_base &stream)
{
	os << "filename: " << stream.file << std::endl;
	os << "block_size: " << stream.block_size << std::endl;
	os << "size: " << stream.size << std::endl;
	os << "call_initialize_region_runtime: " << stream.call_initialize_region_runtime << std::endl;
	os << "call_initialize_region_runtime_after_reopen: " << stream.call_initialize_region_runtime_after_reopen
	   << std::endl;
	os << span_runtimes_from_stream(stream.sut);
	return os;
}

static inline auto make_default_test_stream()
{
	return pmemstream_test_base(get_test_config().filename, get_test_config().block_size,
				    get_test_config().stream_size, true);
}

struct pmemstream_with_single_init_region : public pmemstream_test_base {
	pmemstream_with_single_init_region(pmemstream_test_base &&base, const std::vector<std::string> &data)
	    : pmemstream_test_base(std::move(base))
	{
		helpers.initialize_single_region(TEST_DEFAULT_REGION_SIZE, data);
	}
};

struct pmemstream_with_single_empty_region : public pmemstream_test_base {
	pmemstream_with_single_empty_region(pmemstream_test_base &&base) : pmemstream_test_base(std::move(base))
	{
		helpers.initialize_single_region(TEST_DEFAULT_REGION_SIZE, {});
	}
};

template <bool initialize_region_runtime = false, bool initialize_region_runtime_after_reopen = false>
struct pmemstream_with_multi_empty_regions : public pmemstream_test_base {
	pmemstream_with_multi_empty_regions(pmemstream_test_base &&base, size_t max_regions_count)
	    : pmemstream_test_base(std::move(base))
	{
		// XXX: always initialize for concurrent appends (region_runtime map in helpers is not thread safe)
		stream.call_initialize_region_runtime = initialize_region_runtime;
		stream.call_initialize_region_runtime_after_reopen = initialize_region_runtime_after_reopen;
		stream.helpers.call_region_runtime_initialize = initialize_region_runtime;
		helpers.initialize_multi_regions(max_regions_count, get_test_config().region_size, {});
	}
};

struct pmemstream_empty : public pmemstream_test_base {
	pmemstream_empty(pmemstream_test_base &&base) : pmemstream_test_base(std::move(base))
	{
	}
};

struct pmemstream_with_multi_non_empty_regions : public pmemstream_test_base {
	pmemstream_with_multi_non_empty_regions(pmemstream_test_base &&base,
						const std::vector<std::vector<std::string>> &data)
	    : pmemstream_test_base(std::move(base))
	{
		for (const auto &d : data) {
			helpers.initialize_single_region(TEST_DEFAULT_REGION_MULTI_SIZE, d);
		}
	}
};

#endif /* LIBPMEMSTREAM_STREAM_HELPERS_HPP */
