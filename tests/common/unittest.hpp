// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018-2022, Intel Corporation */

#ifndef LIBPMEMSTREAM_UNITTEST_HPP
#define LIBPMEMSTREAM_UNITTEST_HPP

#include "unittest.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
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

#define UT_ASSERT_UNREACHABLE                                                                                          \
	do {                                                                                                           \
		UT_FATAL("%s:%d in function %s should never be reached", __FILE__, __LINE__, __func__);                \
	} while (0)

namespace predicates
{
template <typename T>
auto equal(const T &expected)
{
	return [&](const auto &value) { return value == expected; };
}
} // namespace predicates

template <typename R, typename Pred>
bool all_of(const R &r, Pred &&pred)
{
	return std::all_of(r.begin(), r.end(), std::forward<Pred>(pred));
}

template <typename R>
bool all_equal(const R &r)
{
	return r.empty() ? true : std::equal(r.begin() + 1, r.end(), r.begin());
}

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
 * } // if f or g returned false, return_check will fire an assert in its dtor
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

/* From: https://stackoverflow.com/a/20846873/5935594 CC BY-SA 3.0
 * It's useful for binding lambdas which capture non-copyable types
 * to std::function.
 */
template <class F>
auto make_copyable_function(F &&f)
{
	using dF = std::decay_t<F>;
	auto spf = std::make_shared<dF>(std::forward<F>(f));
	return [spf](auto &&... args) { return (*spf)(decltype(args)(args)...); };
}

std::unique_ptr<struct pmemstream, std::function<void(struct pmemstream *)>>
make_pmemstream(const std::string &file, size_t block_size, size_t size, bool truncate = true)
{
	struct pmem2_map *map = map_open(file.c_str(), size, truncate);
	if (map == NULL) {
		throw std::runtime_error(pmem2_errormsg());
	}

	auto map_delete = [](struct pmem2_map *map) { pmem2_map_delete(&map); };
	auto map_uptr = std::unique_ptr<struct pmem2_map, decltype(map_delete)>(map, map_delete);

	struct pmemstream *stream;
	int ret = pmemstream_from_map(&stream, block_size, map);
	if (ret == -1) {
		throw std::runtime_error("pmemstream_from_map failed");
	}

	auto stream_delete = [map_uptr = std::move(map_uptr)](struct pmemstream *stream) {
		pmemstream_delete(&stream);
	};
	return std::unique_ptr<struct pmemstream, std::function<void(struct pmemstream *)>>(
		stream, make_copyable_function(std::move(stream_delete)));
}

#endif /* LIBPMEMSTREAM_UNITTEST_HPP */
