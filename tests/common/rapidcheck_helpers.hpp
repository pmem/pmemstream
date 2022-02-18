// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#ifndef LIBPMEMSTREAM_RAPIDCHECK_HELPERS_HPP
#define LIBPMEMSTREAM_RAPIDCHECK_HELPERS_HPP

#include <rapidcheck.h>
#include <rapidcheck/state.h>

#include "stream_helpers.hpp"

struct pmemstream_with_single_init_region {
	pmemstream_with_single_init_region(const std::vector<std::string> &data)
	    : sut(pmemstream_sut(test_config.filename, TEST_DEFAULT_BLOCK_SIZE, TEST_DEFAULT_STREAM_SIZE, true))
	{
		sut.helpers.initialize_single_region(TEST_DEFAULT_REGION_SIZE, data);
	}

	pmemstream_sut sut;
};

struct pmemstream_with_single_empty_region {
	pmemstream_with_single_empty_region()
	    : sut(pmemstream_sut(test_config.filename, TEST_DEFAULT_BLOCK_SIZE, TEST_DEFAULT_STREAM_SIZE, true))
	{
		sut.helpers.initialize_single_region(TEST_DEFAULT_REGION_SIZE, {});
	}

	pmemstream_sut sut;
};

struct pmemstream_empty {
	pmemstream_empty()
	    : sut(pmemstream_sut(test_config.filename, TEST_DEFAULT_BLOCK_SIZE, TEST_DEFAULT_STREAM_SIZE, true))
	{
	}

	pmemstream_sut sut;
};

struct pmemstream_model {
	bool user_created_runtime = false;
	bool user_created_runtime_after_reopen = false;
	std::map<uint64_t, std::vector<std::string>> regions;
};

using pmemstream_command = rc::state::Command<pmemstream_model, pmemstream_sut>;

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

	void run(const pmemstream_model &m, pmemstream_sut &s) const override
	{
		if (m.user_created_runtime) {
			for (auto &region : m.regions)
				s.helpers.region_runtime_initialize(pmemstream_region{region.first});
		}

		for (auto &data : data_to_append) {
			s.helpers.append(pmemstream_region{data.first}, {data.second});
		}

		auto next_model = nextState(m);
		for (auto &data : next_model.regions) {
			s.helpers.verify(pmemstream_region{data.first}, {data.second}, {});
		}
	}
};

struct rc_reopen_command : public pmemstream_command {
	void run(const pmemstream_model &m, pmemstream_sut &s) const override
	{
		s.reopen();
		if (m.user_created_runtime_after_reopen) {
			for (auto &region : m.regions)
				s.helpers.region_runtime_initialize(pmemstream_region{region.first});
		}
	}
};

namespace rc
{

/* XXX: add shrinking support for pmemstream? */
template <>
struct Arbitrary<pmemstream_empty> {
	static Gen<pmemstream_empty> arbitrary()
	{
		return gen::noShrink(gen::construct<pmemstream_empty>());
	}
};

template <>
struct Arbitrary<pmemstream_with_single_empty_region> {
	static Gen<pmemstream_with_single_empty_region> arbitrary()
	{
		return gen::noShrink(gen::construct<pmemstream_with_single_empty_region>());
	}
};

template <>
struct Arbitrary<pmemstream_with_single_init_region> {
	static Gen<pmemstream_with_single_init_region> arbitrary()
	{
		return gen::noShrink(
			gen::construct<pmemstream_with_single_init_region>(gen::arbitrary<std::vector<std::string>>()));
	}
};

} // namespace rc

#endif /* LIBPMEMSTREAM_RAPIDCHECK_HELPERS_HPP */
