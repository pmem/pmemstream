// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <iterator>
#include <random>

namespace benchmark
{
template <typename TimeUnit, typename F>
typename TimeUnit::rep measure(F &&func)
{
	auto start = std::chrono::steady_clock::now();

	func();

	auto duration = std::chrono::duration_cast<TimeUnit>(std::chrono::steady_clock::now() - start);
	return duration.count();
}

template <typename TimeUnit, typename F>
auto measure(size_t iterations, F &&func)
{
	std::vector<typename TimeUnit::rep> results;
	results.reserve(iterations);
	for (size_t i = 0; i < iterations; i++) {
		results.push_back(measure<TimeUnit>(func));
	}
	return results;
}

static inline auto generate_data(size_t count)
{
	static std::mt19937_64 generator = []() {
		std::random_device rd;
		auto seed = rd();
		return std::mt19937_64(seed);
	}();
	size_t gen_size = sizeof(std::mt19937_64::result_type);
	std::vector<uint8_t> ret;
	ret.reserve(count);
	for (size_t i = 0; i < count / gen_size; ++i) {
		auto entry = generator();
		uint8_t *entry_slices = reinterpret_cast<uint8_t *>(&entry);

		for (size_t j = 0; j < gen_size; j++) {
			ret.push_back(entry_slices[j]);
		}
	}

	auto last_entry = generator();
	uint8_t *last_entry_slices = reinterpret_cast<uint8_t *>(&last_entry);

	for (size_t i = 0; i < count % gen_size; i++) {
		ret.push_back(last_entry_slices[i]);
	}
	assert(ret.size() == count);
	return ret;
}

template <typename T>
T min(const std::vector<T> &values)
{
	return *std::min_element(values.begin(), values.end());
}

template <typename T>
T max(const std::vector<T> &values)
{
	return *std::max_element(values.begin(), values.end());
}

template <typename T>
double mean(const std::vector<T> &values)
{
	return std::accumulate(values.begin(), values.end(), 0.0) / values.size();
}

template <typename T>
double std_dev(const std::vector<T> &values)
{
	auto m = mean(values);
	std::vector<T> diff_squares;
	diff_squares.reserve(values.size());

	for (auto &v : values) {
		diff_squares.push_back(std::pow((v - m), 2.0));
	}

	auto variance = std::accumulate(diff_squares.begin(), diff_squares.end(), 0.0) / values.size();

	return std::sqrt(variance);
}

} // namespace benchmark
