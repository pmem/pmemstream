// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#ifndef LIBPMEMSTREAM_RAPIDCHECK_HELPERS_HPP
#define LIBPMEMSTREAM_RAPIDCHECK_HELPERS_HPP

#include <rapidcheck.h>
#include <rapidcheck/state.h>

#include "span.h"
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

			size_t total_entries_size = 0;
			for (auto append : data.second) {
				size_t total_entry_size = sizeof(span_entry) + append.size();
				total_entries_size += ALIGN_UP(total_entry_size, sizeof(span_bytes));
			}

			UT_ASSERTeq(s.sut.region_size(pmemstream_region{data.first}) - total_entries_size,
				    s.sut.region_usable_size(pmemstream_region{data.first}));
		}
	}
};

/* Holds integer value between Min and Max (inclusive) */
template <typename T, size_t Min, size_t Max>
struct ranged {
	static constexpr T min = Min;
	static constexpr T max = Max;
	T value;

	ranged(T val)
	{
		if (val < min || val > max) {
			throw std::runtime_error("Improper value");
		}
		value = val;
	}

	operator T() const
	{
		return value;
	}
};

/* Generators for custom structures. */
namespace rc
{

/* For some reason it's not defined in rapidcheck API
 * XXX: Handle elements count in  test config */
constexpr size_t DEFAULT_ELEMENT_COUNT = 100;

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
struct Arbitrary<pmemstream_with_multi_empty_regions> {
	static Gen<pmemstream_with_multi_empty_regions> arbitrary()
	{
		return gen::noShrink(gen::construct<pmemstream_with_multi_empty_regions>(
			gen::arbitrary<pmemstream_test_base>(),
			gen::inRange<size_t>(1, TEST_DEFAULT_REGION_MULTI_MAX_COUNT)));
	}
};

/* XXX: Pass max data size to generator */
template <>
struct Arbitrary<pmemstream_with_multi_non_empty_regions> {
	static Gen<pmemstream_with_multi_non_empty_regions> arbitrary()
	{
		using RegionT = std::vector<std::string>;
		using StreamT = std::vector<RegionT>;

		const auto region_generator = gen::container<RegionT>(gen::nonEmpty(gen::arbitrary<std::string>()));

		const auto non_empty_region_generator =
			gen::suchThat<RegionT>(region_generator, [](const RegionT &data) { return data.size() > 0; });

		const auto stream_generator = gen::container<StreamT>(non_empty_region_generator);
		/* XXX: Configure number of regions via testconfig */
		const auto constrained_stream_generator =
			gen::suchThat<StreamT>(stream_generator, [](const StreamT &data) {
				return (data.size() > 0) && (data.size() <= TEST_DEFAULT_REGION_MULTI_MAX_COUNT);
			});

		return gen::noShrink(gen::construct<pmemstream_with_multi_non_empty_regions>(
			gen::arbitrary<pmemstream_test_base>(), constrained_stream_generator));
	}
};

/* XXX: Pass max data size to generator */
template <>
struct Arbitrary<pmemstream_with_single_init_region> {
	static Gen<pmemstream_with_single_init_region> arbitrary()
	{
		return gen::noShrink(gen::construct<pmemstream_with_single_init_region>(
			gen::arbitrary<pmemstream_test_base>(), gen::arbitrary<std::vector<std::string>>()));
	}
};

template <typename T, size_t Min, size_t Max>
struct Arbitrary<ranged<T, Min, Max>> {
	static Gen<ranged<T, Min, Max>> arbitrary()
	{
		return gen::construct<ranged<T, Min, Max>>(gen::inRange<T>(Min, Max + 1));
	}
};

} // namespace rc

#endif /* LIBPMEMSTREAM_RAPIDCHECK_HELPERS_HPP */
