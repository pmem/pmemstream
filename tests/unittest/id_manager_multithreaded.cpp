// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

/* id_manager_multithreaded.cpp -- verifies correctness of id_manager module in multithreaded env. */

#include "id_manager.h"

#include <algorithm>
#include <numeric>
#include <vector>

#include "thread_helpers.hpp"
#include "unittest.hpp"

namespace
{
static constexpr size_t concurrency = 24;

auto make_id_manager = make_instance_ctor(id_manager_new, id_manager_destroy);
} // namespace

int main(int argc, char *argv[])
{
	return run_test([] {
		/* Simple multi-threaded test. */

		static constexpr size_t num_ops_per_thread_base = 16;
		auto manager = make_id_manager();

		std::vector<std::vector<uint64_t>> thread_ids(concurrency);
		parallel_exec(concurrency, [&](size_t thread_id) {
			auto num_ops = num_ops_per_thread_base * thread_id;
			for (size_t i = 0; i < num_ops; i++) {
				thread_ids[thread_id].push_back(id_manager_acquire(manager.get()));
			}

			for (auto id : thread_ids[thread_id]) {
				int ret = id_manager_release(manager.get(), id);
				UT_ASSERT(ret == 0);
			}
		});

		auto merged_ids = std::accumulate(thread_ids.begin(), thread_ids.end(), std::vector<uint64_t>{},
						  [](auto &&lhs, const auto &rhs) {
							  lhs.insert(lhs.end(), rhs.begin(), rhs.end());
							  return std::move(lhs);
						  });
		std::sort(merged_ids.begin(), merged_ids.end());

		/* The id space should be contiguous. */
		for (size_t i = 1; i < merged_ids.size(); i++) {
			UT_ASSERT(merged_ids[i] >= merged_ids[i - 1]);
		}
	});
}
