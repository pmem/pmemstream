// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018-2022, Intel Corporation */

#ifndef LIBPMEMSTREAM_UNITTEST_HPP
#define LIBPMEMSTREAM_UNITTEST_HPP

#include "unittest.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <type_traits>

#include "env_setter.hpp"
#include "valgrind_internal.h"

/* Execute only this many runs of rc_check tests under valgrind. It must be bigger than one because
 * the first run is usually executed with size == 0. */
static constexpr size_t RAPIDCHECK_MAX_SUCCESS_ON_VALGRIND = 5;

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

struct test_config_type {
	std::string filename = "";
	size_t max_concurrency = std::numeric_limits<size_t>::max() - 1;
	size_t stream_size = TEST_DEFAULT_STREAM_SIZE;
	size_t block_size = TEST_DEFAULT_BLOCK_SIZE;
	/* all regions are required to have the same size */
	size_t region_size = TEST_DEFAULT_REGION_MULTI_SIZE;
	size_t regions_count = TEST_DEFAULT_REGION_MULTI_MAX_COUNT;
	std::map<std::string, std::string> rc_params;
};

static const test_config_type &get_test_config()
{
	static test_config_type test_config;
	return test_config;
}

static inline int run_test(test_config_type config, std::function<void()> test)
{
	test_register_sighandlers();
	set_valgrind_internals();

	if (On_valgrind && config.rc_params.count("max_success") == 0)
		config.rc_params["max_success"] = std::to_string(RAPIDCHECK_MAX_SUCCESS_ON_VALGRIND);

	if (On_valgrind)
		config.max_concurrency = TEST_DEFAULT_MAX_CONCURRENCY_ON_VALGRIND;

	std::string rapidcheck_config;
	for (auto &kv : config.rc_params)
		rapidcheck_config += kv.first + "=" + kv.second + " ";

	const_cast<test_config_type &>(get_test_config()) = config;

	try {
		env_setter setter("RC_PARAMS", rapidcheck_config, false);
		test();
	} catch (std::exception &e) {
		UT_EXCEPTION(e);
		abort();
	} catch (...) {
		std::cerr << "catch(...){}" << std::endl;
		abort();
	}

	return 0;
}

static inline int run_test(std::function<void()> test)
{
	return run_test({}, test);
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
		if (!status)
			abort();
	}

	return_check &operator+=(bool rhs)
	{
		if (!rhs)
			status = false;

		return *this;
	}

	bool status = true;
};

/* Return function which constructs (creates unique_ptr) an instance of an object using ctor().
 * Its main purpose is to wrap C-like interface with _new and _destroy functions in unique_ptr. */
template <typename Ctor, typename Dtor>
auto make_instance_ctor(Ctor &&ctor, Dtor &&dtor)
{
	return [&](auto &&...args) {
		auto ptr = std::unique_ptr<std::remove_reference_t<decltype(*ctor(args...))>, decltype(&dtor)>(
			ctor(args...), &dtor);
		if (!ptr) {
			throw std::runtime_error("Ctor failed!");
		}
		return ptr;
	};
}

static inline std::filesystem::path copy_file(std::string path)
{
	std::filesystem::path copy_path = path;
	copy_path += ".cpy";
	std::filesystem::copy_file(path, copy_path, std::filesystem::copy_options::overwrite_existing);
	return copy_path;
}

#endif /* LIBPMEMSTREAM_UNITTEST_HPP */
