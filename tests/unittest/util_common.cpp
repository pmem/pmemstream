// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

/* util_common.cpp -- tests for common helpers in util.c */

#include "common/util.h"

#include <rapidcheck.h>

#include "unittest.hpp"

int main()
{
	struct test_config_type test_config;

	return run_test(test_config, [] {
		return_check ret;

		ret += rc::check("check if IS_POW2 == (x == 1 << log2(x))", [](const size_t value) {
			UT_ASSERTeq(IS_POW2(value), (value == (1UL << log2_uint(value))));
		});
	});
}
