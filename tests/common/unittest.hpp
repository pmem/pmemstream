// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018-2021, Intel Corporation */

#ifndef PMEMSTREAM_UNITTEST_HPP
#define PMEMSTREAM_UNITTEST_HPP

#include "unittest.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <type_traits>

static inline void UT_EXCEPTION(std::exception &e)
{
	std::cerr << e.what() << std::endl;
}

/* assertion with exception related string printed */
#define UT_FATALexc(exception)                                                                                         \
	((void)(UT_EXCEPTION(exception), (UT_FATAL("%s:%d %s - assertion failure", __FILE__, __LINE__, __func__), 0)))

#ifdef _WIN32
#define __PRETTY_FUNCTION__ __func__
#endif
#define PRINT_TEST_PARAMS                                                                                              \
	do {                                                                                                           \
		std::cout << "TEST: " << __PRETTY_FUNCTION__ << std::endl;                                             \
	} while (0)

#define STAT(path, st) ut_stat(__FILE__, __LINE__, __func__, path, st)

#define UT_ASSERT_NOEXCEPT(...) static_assert(noexcept(__VA_ARGS__), "Operation must be noexcept")

#define ASSERT_UNREACHABLE                                                                                             \
	do {                                                                                                           \
		UT_FATAL("%s:%d in function %s should never be reached", __FILE__, __LINE__, __func__);                \
	} while (0)

static inline int run_test(std::function<void()> test)
{
	test_register_sighandlers();

	try {
		test();
	} catch (std::exception &e) {
		UT_FATALexc(e);
	} catch (...) {
		UT_FATAL("catch(...){}");
	}

	return 0;
}

/* Helper structure for verifying return values from function.
 * Usage:
 *
 * {
 *     return_check check;
 *     check += f();
 *     check += g();
 * } // if f or g returned false, return_check will fire and assert in it's dtor
 */
struct return_check {
	~return_check()
	{
		UT_ASSERT(status);
	}

	return_check &operator+=(bool rhs)
	{
		if (!rhs)
			status = false;

		return *this;
	}

	bool status = true;
};

#endif /* PMEMSTREAM_UNITTEST_HPP */
