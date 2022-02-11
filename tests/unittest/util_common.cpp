// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

/* util_popcount.cpp -- verifies correctness of util_popcount_memory calculations */

#include "common/util.h"

#include <rapidcheck.h>

#include "unittest.hpp"

int main()
{
	return run_test([] {
		return_check ret;

		ret += rc::check("check if IS_POW2 == (x == 1 << log2(x))", [](const size_t value) {
			UT_ASSERTeq(IS_POW2(value), (value == (1UL << log2_uint(value))));
		});
	});
}
