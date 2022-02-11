// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

/* id_manager.cpp -- verifies correctness of id_manager module */

#include "id_manager.h"

#include <vector>

#include <rapidcheck.h>

#include "thread_helpers.hpp"
#include "unittest.hpp"

namespace
{
static constexpr size_t max_num_id_requests = 1024;

auto make_id_manager = make_instance_ctor(id_manager_new, id_manager_destroy);

template <typename It>
void release_ids(id_manager *manager, size_t size, It &&it)
{
	for (size_t i = 0; i < size; i++) {
		int ret = id_manager_release(manager, *it);
		UT_ASSERTeq(ret, 0);
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
				auto id = id_manager_acquire(manager.get());
				UT_ASSERTeq(id, i);
			}
		}

		/* Verify if ids are reused (in case of one id being used). */
		{
			auto manager = make_id_manager();

			for (size_t i = 0; i < max_num_id_requests; i++) {
				auto id = id_manager_acquire(manager.get());
				UT_ASSERTeq(id, 0);

				int ret = id_manager_release(manager.get(), id);
				UT_ASSERTeq(ret, 0);
			}
		}

		ret += rc::check("verify if ids are reused", [](bool release_from_biggest) {
			/* Generate a vector of acquire/release operations.
			 * XXX: create a dedicated generator for this. */
			static constexpr size_t max_acquire_release_ops = 1024;
			std::vector<std::pair<unsigned, unsigned>> acquire_release;
			size_t num_elements = *rc::gen::inRange<size_t>(1, max_num_id_requests);
			auto gen_acq_rel = rc::gen::inRange<unsigned>(0, max_acquire_release_ops);
			for (size_t i = 0; i < num_elements; i++) {
				acquire_release.emplace_back(*gen_acq_rel, *gen_acq_rel);
			}

			auto manager = make_id_manager();
			std::vector<uint64_t> acquired_ids;

			for (auto &p : acquire_release) {
				auto to_acquire = p.first;
				auto to_release = p.second;

				for (unsigned i = 0; i < to_acquire; i++) {
					auto id = id_manager_acquire(manager.get());

					/* Id will either be bigger than all existing (==
					 * acquired_ids.size()) or will be a reused one. */
					UT_ASSERT(id <= acquired_ids.size());

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
				UT_ASSERT(std::is_sorted(acquired_ids.begin(), acquired_ids.end()));
			}
		});

		ret += rc::check("verify if reacquired ids are assigned in increasing order", []() {
			unsigned ids_to_acquire = *rc::gen::inRange<unsigned>(0, max_num_id_requests);
			auto manager = make_id_manager();
			for (unsigned i = 0; i < ids_to_acquire; i++) {
				auto id = id_manager_acquire(manager.get());
				UT_ASSERTeq(id, i);
			}

			auto to_release =
				*rc::gen::unique<std::vector<unsigned>>(rc::gen::inRange<unsigned>(0, ids_to_acquire));
			for (auto id : to_release) {
				UT_ASSERTeq(id_manager_release(manager.get(), id), 0);
			}

			std::vector<unsigned> reacquired;
			for (unsigned i = 0; i < to_release.size(); i++) {
				auto id = id_manager_acquire(manager.get());
				reacquired.push_back(id);
			}

			std::sort(to_release.begin(), to_release.end());
			UT_ASSERT(std::equal(reacquired.begin(), reacquired.end(), to_release.begin()));
		});
	});
}
