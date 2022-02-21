// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#ifndef LIBPMEMSTREAM_RAPIDCHECK_HELPERS_HPP
#define LIBPMEMSTREAM_RAPIDCHECK_HELPERS_HPP

#include <rapidcheck.h>
#include <rapidcheck/state.h>

#include "stream_helpers.hpp"

/*
 * Definition of models and commands for stateful testing.
 * Ref: https://github.com/emil-e/rapidcheck/blob/master/doc/state.md
 */
struct pmemstream_model {
	std::map<uint64_t, std::vector<std::string>> regions;
};

using pmemstream_command = rc::state::Command<pmemstream_model, pmemstream_test_base>;

/* Command which appends data to all existing regions (present in model). */
struct rc_append_command : public pmemstream_command {
	std::map<uint64_t, std::string> data_to_append;

	explicit rc_append_command(const pmemstream_model &m)
	{
		for (auto &region : m.regions) {
			data_to_append[region.first] = *rc::gen::arbitrary<std::string>();
		}
	}

	void apply(pmemstream_model &m) const override
	{
		for (auto &data : data_to_append) {
			auto &region = m.regions[data.first];
			region.push_back(data.second);
		}
	}

	void run(const pmemstream_model &m, pmemstream_test_base &s) const override
	{
		for (auto &data : data_to_append) {
			s.helpers.append(pmemstream_region{data.first}, {data.second});
		}
	}
};

struct rc_reopen_command : public pmemstream_command {
	void run(const pmemstream_model &m, pmemstream_test_base &s) const override
	{
		s.reopen();
	}
};

struct rc_verify_command : public pmemstream_command {
	void run(const pmemstream_model &m, pmemstream_test_base &s) const override
	{
		for (auto data : m.regions) {
			s.helpers.verify(pmemstream_region{data.first}, {data.second}, {});
		}
	}
};

/* Generators for custom structures. */
namespace rc
{

/* XXX: add shrinking support for pmemstream? */
template <>
struct Arbitrary<pmemstream_test_base> {
	static Gen<pmemstream_test_base> arbitrary()
	{
		return gen::noShrink(gen::construct<pmemstream_test_base>(
			gen::just(get_test_config().filename), gen::just(get_test_config().block_size),
			gen::just(get_test_config().stream_size), gen::just(true), gen::arbitrary<bool>(),
			gen::arbitrary<bool>()));
	}
};

template <>
struct Arbitrary<pmemstream_empty> {
	static Gen<pmemstream_empty> arbitrary()
	{
		return gen::noShrink(gen::construct<pmemstream_empty>(gen::arbitrary<pmemstream_test_base>()));
	}
};

template <>
struct Arbitrary<pmemstream_with_single_empty_region> {
	static Gen<pmemstream_with_single_empty_region> arbitrary()
	{
		return gen::noShrink(
			gen::construct<pmemstream_with_single_empty_region>(gen::arbitrary<pmemstream_test_base>()));
	}
};

template <>
struct Arbitrary<pmemstream_with_single_init_region> {
	static Gen<pmemstream_with_single_init_region> arbitrary()
	{
		return gen::noShrink(gen::construct<pmemstream_with_single_init_region>(
			gen::arbitrary<pmemstream_test_base>(), gen::arbitrary<std::vector<std::string>>()));
	}
};

} // namespace rc

#endif /* LIBPMEMSTREAM_RAPIDCHECK_HELPERS_HPP */
