// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

/* id_manager.cpp -- verifies correctness of id_manager module */

#include "id_manager.h"

#include <set>
#include <vector>

#include <rapidcheck.h>
#include <rapidcheck/state.h>

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

struct id_wrapper {
	id_wrapper() : sut(make_id_manager())
	{
	}

	decltype(make_id_manager()) sut;
};

struct id_model {
	id_model()
	{
		for (size_t i = 0; i < max_num_id_requests; i++) {
			available.insert(i);
		}
	}

	std::set<uint64_t> available;
	std::set<uint64_t> used;
};

using rc_id_command = rc::state::Command<id_model, id_wrapper>;

struct rc_acquire_id_command : public rc_id_command {

	void checkPreconditions(const id_model &m) const override
	{
		RC_PRE(m.available.size() > 0);
	}

	void apply(id_model &m) const override
	{
		/* Prints distribution of m.used.size():
		 * https://github.com/emil-e/rapidcheck/blob/master/doc/distribution.md */
		RC_TAG(m.used.size());

		auto id = *m.available.begin();
		m.available.erase(id);
		m.used.insert(id);
	}

	void run(const id_model &m, id_wrapper &id) const override
	{
		auto acquired_id = id_manager_acquire(id.sut.get());

		/* ids were assigned from smallest available. */
		UT_ASSERTeq(acquired_id, *m.available.begin());

		/* id was not used before */
		UT_ASSERT(m.used.count(acquired_id) == 0);
	}
};

struct rc_release_id_command : public rc_id_command {

	size_t to_release;

	explicit rc_release_id_command(const id_model &m)
	{
		to_release = *rc::gen::elementOf(m.used);
	}

	void checkPreconditions(const id_model &m) const override
	{
		RC_PRE(m.used.count(to_release) == 1);
	}

	void apply(id_model &m) const override
	{
		m.used.erase(to_release);
		m.available.insert(to_release);
	}

	void run(const id_model &m, id_wrapper &id) const override
	{
		id_manager_release(id.sut.get(), to_release);
	}
};

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

		ret += rc::check("verify if ids are reused", []() {
			id_wrapper id{};
			rc::state::check(
				id_model{}, id,
				rc::state::gen::execOneOfWithArgs<rc_acquire_id_command, rc_release_id_command>());
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
