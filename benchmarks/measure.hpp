// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <chrono>
#include <cmath>
#include <iterator>
#include <random>

namespace benchmark
{
template <typename TimeUnit, typename F>
static typename TimeUnit::rep measure(F &&func)
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

auto generate_data(size_t count)
{
	static std::mt19937_64 generator = []() {
		std::random_device rd;
		auto seed = rd();
		return std::mt19937_64(seed);
	}();
	std::vector<uint8_t> ret;
	ret.reserve(count);
	for (size_t i = 0; i < count; ++i) {
		/* only even keys will be insterted */
		auto entry = static_cast<size_t>(generator());

		ret.push_back(entry);
	}
	return ret;
}

template <typename T>
T min(std::vector<T> values)
{
	return *std::min_element(values.begin(), values.end());
}

template <typename T>
T max(std::vector<T> values)
{
	return *std::max_element(values.begin(), values.end());
}

template <typename T>
float mean(std::vector<T> values)
{
	return std::accumulate(values.begin(), values.end(), 0.0) / values.size();
}

template <typename T>
float std_dev(std::vector<T> values)
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
