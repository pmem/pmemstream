// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018-2021, Intel Corporation */

#ifndef PMEMSTREAM_UNITTEST_HPP
#define PMEMSTREAM_UNITTEST_HPP

#include "unittest.h"

#include <pmemstream.h>

#include <condition_variable>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

static inline void UT_EXCEPTION(std::exception &e)
{
	std::cerr << e.what() << std::endl;
}

/* assertion with exception related string printed */
#define UT_FATALexc(exception)                                                           \
	((void)(UT_EXCEPTION(exception),                                                 \
		(UT_FATAL("%s:%d %s - assertion failure", __FILE__, __LINE__, __func__), \
		 0)))

#ifdef _WIN32
#define __PRETTY_FUNCTION__ __func__
#endif
#define PRINT_TEST_PARAMS                                                                \
	do {                                                                             \
		std::cout << "TEST: " << __PRETTY_FUNCTION__ << std::endl;               \
	} while (0)

#define STAT(path, st) ut_stat(__FILE__, __LINE__, __func__, path, st)

#define UT_ASSERT_NOEXCEPT(...)                                                          \
	static_assert(noexcept(__VA_ARGS__), "Operation must be noexcept")

#define STR(x) #x

#define ASSERT_ALIGNED_BEGIN(type, ref)                                                  \
	do {                                                                             \
		size_t off = 0;                                                          \
		const char *last = "(none)";

#define ASSERT_ALIGNED_FIELD(type, ref, field)                                               \
	do {                                                                                 \
		if (offsetof(type, field) != off)                                            \
			UT_FATAL(                                                            \
				"%s: padding, missing field or fields not in order between " \
				"'%s' and '%s' -- offset %lu, real offset %lu",              \
				STR(type), last, STR(field), off,                            \
				offsetof(type, field));                                      \
		off += sizeof((ref).field);                                                  \
		last = STR(field);                                                           \
	} while (0)

#define ASSERT_FIELD_SIZE(field, ref, size)                                              \
	do {                                                                             \
		UT_COMPILE_ERROR_ON(size != sizeof((ref).field));                        \
	} while (0)

#define ASSERT_ALIGNED_CHECK(type)                                                       \
	if (off != sizeof(type))                                                         \
		UT_FATAL("%s: missing field or padding after '%s': "                     \
			 "sizeof(%s) = %lu, fields size = %lu",                          \
			 STR(type), last, STR(type), sizeof(type), off);                 \
	}                                                                                \
	while (0)

#define ASSERT_OFFSET_CHECKPOINT(type, checkpoint)                                       \
	do {                                                                             \
		if (off != checkpoint)                                                   \
			UT_FATAL("%s: violated offset checkpoint -- "                    \
				 "checkpoint %lu, real offset %lu",                      \
				 STR(type), checkpoint, off);                            \
	} while (0)

#define ASSERT_UNREACHABLE                                                               \
	do {                                                                             \
		UT_FATAL("%s:%d in function %s should never be reached", __FILE__,       \
			 __LINE__, __func__);                                            \
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

#endif /* PMEMSTREAM_UNITTEST_HPP */
