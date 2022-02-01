// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <map>
#include <numeric>
#include <vector>

#include <rapidcheck.h>

#include "env_setter.hpp"
#include "mpmc_queue.h"
#include "thread_helpers.hpp"
#include "unittest.hpp"

namespace {
auto make_mpmc_queue = make_instance_ctor(mpmc_queue_new, mpmc_queue_destroy);
auto make_mpmc_queue_snapshot = make_instance_ctor(mpmc_queue_copy, mpmc_queue_destroy);

// XXX: we could use vector of ranges here if they were available...
template <typename T>
std::vector<std::vector<T>> random_divide_data(const std::vector<T> &input, size_t into_num)
{
	using difference_type = typename std::vector<std::vector<T>>::difference_type;
	UT_ASSERT(into_num > 0);

	if (into_num == 1) {
		return {input};
	}

	std::vector<std::vector<T>> ret;
	difference_type divided_size = 0;
	for (size_t i = 0; i < into_num - 1; i++) {
		auto remaining_size = static_cast<difference_type>(input.size()) - divided_size;
		auto size = *rc::gen::inRange<difference_type>(0, remaining_size);
		ret.emplace_back(input.begin() + divided_size, input.begin() + divided_size + size);
		divided_size += size;
	}

	/* Last part is all what remains. */
	ret.emplace_back(input.begin() + divided_size, input.end());

	return ret;
}
}
