// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <getopt.h>
#include <iostream>
#include <vector>

#include "measure.hpp"
/* XXX: Change this header when make_pmemstream moved to public API */
#include "unittest.hpp"

class Config {
 private:
	static constexpr option long_options[] = {{"path", required_argument, NULL, 'p'},
						  {"stream_size", required_argument, NULL, 'x'},
						  {"block_size", required_argument, NULL, 'b'},
						  {"region_size", required_argument, NULL, 'r'},
						  {"element_count", required_argument, NULL, 'c'},
						  {"element_size", required_argument, NULL, 's'},
						  {"iterations", required_argument, NULL, 'i'},
						  {"null_region_runtime", no_argument, NULL, 'n'},
						  {"help", no_argument, NULL, 'h'},
						  {NULL, 0, NULL, 0}};

	std::string app_name;

 public:
	std::string path;
	size_t stream_size = TEST_DEFAULT_STREAM_SIZE;
	size_t block_size = TEST_DEFAULT_BLOCK_SIZE;
	size_t region_size = TEST_DEFAULT_REGION_SIZE;
	size_t element_count = 100000;
	size_t element_size = 1024;
	size_t iterations = 10;
	bool null_region_runtime = false;

	int parse_arguments(int argc, char *argv[])
	{
		app_name = std::string(argv[0]);
		int ch;
		while ((ch = getopt_long(argc, argv, "p:x:b:r:c:s:i:nh", long_options, NULL)) != -1) {
			switch (ch) {
				case 'p':
					path = std::string(optarg);
					break;
				case 'x':
					stream_size = std::stoull(optarg);
					break;
				case 'b':
					block_size = std::stoull(optarg);
					break;
				case 'r':
					region_size = std::stoull(optarg);
					break;
				case 'c':
					element_count = std::stoull(optarg);
					break;
				case 's':
					element_size = std::stoull(optarg);
					break;
				case 'i':
					iterations = std::stoull(optarg);
					break;
				case 'n':
					null_region_runtime = true;
					break;
				case 'h':
					return -1;
				default:
					throw std::invalid_argument("Invalid argument");
			}
		}
		if (path.empty()) {
			throw std::invalid_argument("Please provide path");
		}
		return 0;
	}

	std::string usage()
	{
		return "Usage:" + app_name + "[OPTION]...\n" + "Pmemstream benchmark for append\n" +
			"\n\t--path [path] path to file" + "\n\t--stream_size [size] stream size" +
			"\n\t--block size [size] block size" + "\n\t--region_size [size] region size" +
			"\n\t--element_count [count] Number of elements inserted into stream" +
			"\n\t--element_size [size] Number of bytes of each element inserted into stream" +
			"\n\t--iterations [iterations] Number of iterations. " +
			"More iterations gives more robust statistical data, but takes more time" +
			"\n\t--null_region_runtime indicates if **null** region runtime would be passed to append" +
			"\n\t--help display this message";
	}
};
constexpr option Config::long_options[];

std::ostream &operator<<(std::ostream &out, Config const &cfg)
{
	out << "Pmemstream Benchmark, path: " << cfg.path << ", ";
	out << "stream_size: " << cfg.stream_size << ", ";
	out << "block_size: " << cfg.block_size << ", ";
	out << "region_size: " << cfg.region_size << ", ";
	out << "element_count: " << cfg.element_count << ", ";
	out << "element_size: " << cfg.element_size << ", ";
	out << "null_region_runtime: " << std::boolalpha << cfg.null_region_runtime << ", ";
	out << "Number of iterations: " << cfg.iterations;
	return out;
}

int main(int argc, char *argv[])
{
	// auto config = parse_arguments(argc, argv);
	Config config;
	try {
		if (config.parse_arguments(argc, argv) != 0) {
			std::cout << config.usage() << std::endl;
			exit(0);
		}
	} catch (std::invalid_argument const &e) {
		std::cerr << e.what() << std::endl;
		exit(1);
	}
	std::cout << config << std::endl;

	auto bytes_to_generate = config.element_count * config.element_size;

	std::vector<uint64_t> data;
	auto input_generation_time =
		benchmark::measure<std::chrono::seconds>([&] { data = benchmark::generate_data(bytes_to_generate); });

	std::cout << "input generation time: " << input_generation_time << "s" << std::endl;

	auto stream = make_pmemstream(config.path.c_str(), config.block_size, config.stream_size);
	struct pmemstream_region region;
	if (pmemstream_region_allocate(stream.get(), config.region_size, &region)) {
		return -1;
	}

	pmemstream_region_runtime *region_runtime_ptr = NULL;
	if (!config.null_region_runtime) {
		if (pmemstream_get_region_runtime(stream.get(), region, &region_runtime_ptr)) {
			return -2;
		}
	}

	/* XXX: Add initialization phase whith separate measurement */
	auto results = benchmark::measure<std::chrono::nanoseconds>(config.iterations, [&] {
		for (size_t i = 0; i < data.size(); i += config.element_size) {
			pmemstream_append(stream.get(), region, region_runtime_ptr, data.data() + i,
					  config.element_size, NULL);
		}
	});

	auto mean = benchmark::mean(results) / config.element_count;
	auto max = static_cast<size_t>(benchmark::max(results)) / config.element_count;
	auto min = static_cast<size_t>(benchmark::min(results)) / config.element_count;
	auto std_dev = benchmark::std_dev(results) / config.element_count;

	std::cout << "pmemstream_append measurement:" << std::endl;
	std::cout << "\tmean: " << mean << "ns" << std::endl;
	std::cout << "\tmax: " << max << "ns" << std::endl;
	std::cout << "\tmin: " << min << "ns" << std::endl;
	std::cout << "\tstandard deviation: " << std_dev << "ns" << std::endl;
}
