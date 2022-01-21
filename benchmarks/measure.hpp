// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <algorithm>
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

/* Generate vector<uint64_t> which contains byte_count rounded up to 64 bits*/
static inline std::vector<uint64_t> generate_data(size_t bytes_count)
{
	size_t count = bytes_count / 8 + (bytes_count % 8 != 0);
	std::vector<uint64_t> ret;
	ret.reserve(count);

	static std::mt19937_64 generator = []() {
		std::random_device rd;
		auto seed = rd();
		return std::mt19937_64(seed);
	}();

	for (size_t i = 0; i < count; ++i) {
		ret.push_back(generator());
	}

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
