// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

/* id_manager.cpp -- verifies correctness of id_manager module */

#include "id_manager.h"

#include <algorithm>
#include <memory>
#include <numeric>
#include <variant>
#include <vector>

#include <rapidcheck.h>

#include "thread_helpers.hpp"
#include "unittest.hpp"

namespace
{
static constexpr size_t max_num_id_requests = 1024;
static constexpr size_t concurrency = 128;

auto make_id_manager()
{
	return std::unique_ptr<id_manager, decltype(&id_manager_destroy)>(id_manager_new(), &id_manager_destroy);
}

template <typename It>
void release_ids(id_manager *manager, size_t size, It &&it)
{
	for (size_t i = 0; i < size; i++) {
		int ret = id_manager_release_id(manager, *it);
		RC_ASSERT(ret == 0);
		++it;
	}
}

} // namespace

int main()
{
	return run_test([] {
		return_check ret;

		/* Verify if ids are returned in an increasing order. */
		{
			auto manager = make_id_manager();

			for (size_t i = 0; i < max_num_id_requests; i++) {
				auto id = id_manager_acquire_id(manager.get());
				UT_ASSERT(id == i);
			}
		}

		/* Verify if ids are reused (in case of one id being used). */
		{
			auto manager = make_id_manager();

			for (size_t i = 0; i < max_num_id_requests; i++) {
				auto id = id_manager_acquire_id(manager.get());
				UT_ASSERT(id == 0);

				int ret = id_manager_release_id(manager.get(), id);
				UT_ASSERT(ret == 0);
			}
		}

		/* Simple multi-threaded test. */
		{
			static constexpr size_t num_ops_per_thread_base = 16;
			auto manager = make_id_manager();

			std::vector<std::vector<uint64_t>> thread_ids(concurrency);
			parallel_exec(concurrency, [&](size_t thread_id) {
				auto num_ops = num_ops_per_thread_base * thread_id;
				for (size_t i = 0; i < num_ops; i++) {
					thread_ids[thread_id].push_back(id_manager_acquire_id(manager.get()));
				}

				for (auto id : thread_ids[thread_id]) {
					int ret = id_manager_release_id(manager.get(), id);
					UT_ASSERT(ret == 0);
				}
			});

			auto merged_ids = std::accumulate(thread_ids.begin(), thread_ids.end(), std::vector<uint64_t>{},
							  [](auto &&lhs, const auto &rhs) {
								  lhs.insert(lhs.end(), rhs.begin(), rhs.end());
								  return std::move(lhs);
							  });
			std::sort(merged_ids.begin(), merged_ids.end());

			/* The id space should be contigious. */
			for (size_t i = 1; i < merged_ids.size(); i++) {
				UT_ASSERT(merged_ids[i] >= merged_ids[i]);
			}
		}

		ret += rc::check("verify if ids are reused", [](bool release_from_biggest) {
			/* Generate a vector of acquire/release operations. */
			static constexpr size_t max_acquire_release_ops = 1024;
			std::vector<std::pair<unsigned, unsigned>> acquire_release;
			size_t num_elements = *rc::gen::inRange<size_t>(1, max_num_id_requests);
			auto gen_acq_rel = rc::gen::inRange<size_t>(0, max_acquire_release_ops);
			for (size_t i = 0; i < num_elements; i++) {
				acquire_release.emplace_back(*gen_acq_rel, *gen_acq_rel);
			}

			auto manager = make_id_manager();
			std::vector<uint64_t> acquired_ids;

			for (auto &p : acquire_release) {
				auto to_acquire = p.first;
				auto to_release = p.second;

				for (size_t i = 0; i < to_acquire; i++) {
					auto id = id_manager_acquire_id(manager.get());

					/* Id will either be bigger than all existsing (==
					 * acquired_ids.size()) or will be a reused one. */
					RC_ASSERT(id <= acquired_ids.size());

					acquired_ids.emplace_back(id);
				}

				if (acquired_ids.size() < to_release)
					to_release = acquired_ids.size();

				if (release_from_biggest) {
					release_ids(manager.get(), to_release, acquired_ids.rbegin());
				} else {
					release_ids(manager.get(), to_release, acquired_ids.begin());
					/* Move remaining items into the place of released ones. */
					std::rotate(acquired_ids.begin(),
						    acquired_ids.begin() + static_cast<int64_t>(to_release),
						    acquired_ids.end());
				}
				acquired_ids.resize(acquired_ids.size() - to_release);
			}

			if (release_from_biggest) {
				/* Since release was always done in reverse order, all ids should be
				 * sorted. */
				RC_ASSERT(std::is_sorted(acquired_ids.begin(), acquired_ids.end()));
			}
		});
	});
}
