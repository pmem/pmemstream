// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iterator>
#include <random>

namespace benchmark
{

class workload_base {
 public:
	virtual ~workload_base(){};
	virtual void initialize() = 0;
	virtual void perform() = 0;
	virtual void clean() = 0;

	void prepare_data(size_t bytes_to_generate)
	{
		data = generate_data(bytes_to_generate);
	}
	uint8_t *get_data_chunks()
	{
		return reinterpret_cast<uint8_t *>(data.data());
	}

 protected:
	std::vector<uint64_t> data;

	/* Generate vector<uint64_t> which contains byte_count rounded up to 64 bits*/
	std::vector<uint64_t> generate_data(size_t bytes_count)
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
};

template <typename TimeUnit, typename F>
typename TimeUnit::rep measure(F &&func)
{
	auto start = std::chrono::steady_clock::now();

	func();

	auto duration = std::chrono::duration_cast<TimeUnit>(std::chrono::steady_clock::now() - start);
	return duration.count();
}

/* Measure time of execution of run_workload function. init() and clean()
 * functions are executed respectively before and after each iteration */
template <typename TimeUnit>
auto measure(size_t iterations, workload_base *workload)
{
	std::vector<typename TimeUnit::rep> results;
	results.reserve(iterations);

	for (size_t i = 0; i < iterations; i++) {
		workload->initialize();
		results.push_back(measure<TimeUnit>([&]() { workload->perform(); }));
		workload->clean();
	}

	return results;
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
