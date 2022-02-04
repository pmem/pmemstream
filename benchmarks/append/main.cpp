// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <forward_list>
#include <getopt.h>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

#include "measure.hpp"
/* XXX: Change this header when make_pmemstream moved to public API */
#include "unittest.hpp"

#include <libpmemlog.h>

class config {
 private:
	static constexpr std::array<std::string_view, 2> engine_names = {"pmemlog", "pmemstream"};
	static constexpr option long_options[] = {{"engine", required_argument, NULL, 'e'},
						  {"path", required_argument, NULL, 'p'},
						  {"size", required_argument, NULL, 'x'},
						  {"block_size", required_argument, NULL, 'b'},
						  {"region_size", required_argument, NULL, 'r'},
						  {"element_count", required_argument, NULL, 'c'},
						  {"element_size", required_argument, NULL, 's'},
						  {"iterations", required_argument, NULL, 'i'},
						  {"null_region_runtime", no_argument, NULL, 'n'},
						  {"help", no_argument, NULL, 'h'},
						  {NULL, 0, NULL, 0}};

	static std::string app_name;

	static std::string available_engines()
	{
		std::string possible_engines;
		for (auto name : engine_names) {
			possible_engines += std::string(name) + " ";
		}
		return possible_engines;
	}

	void set_engine(std::string engine_name)
	{
		if (std::find(engine_names.begin(), engine_names.end(), engine) == engine_names.end()) {
			throw std::invalid_argument(std::string("Wrong engine name, possible: ") + available_engines());
		}
		engine = engine_name;
	}

	void set_size(size_t size_arg)
	{
		if (size_arg < size) {
			throw std::invalid_argument(std::string("Invalid size, should be >=") + std::to_string(size));
		}
		size = size_arg;
	}

 public:
	std::string engine = "pmemstream";
	std::string path;
	size_t size = std::max(PMEMLOG_MIN_POOL, TEST_DEFAULT_STREAM_SIZE * 10);
	size_t block_size = TEST_DEFAULT_BLOCK_SIZE * 10;
	size_t region_size = TEST_DEFAULT_REGION_SIZE * 10;
	size_t element_count = 100;
	size_t element_size = 1024;
	size_t iterations = 10;
	bool null_region_runtime = false;

	int parse_arguments(int argc, char *argv[])
	{
		app_name = std::string(argv[0]);
		int ch;
		while ((ch = getopt_long(argc, argv, "e:p:x:b:r:c:s:i:nh", long_options, NULL)) != -1) {
			switch (ch) {
				case 'e':
					set_engine(std::string(optarg));
					break;
				case 'p':
					path = std::string(optarg);
					break;
				case 'x':
					set_size(std::stoull(optarg));
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

	static void print_usage()
	{
		std::vector<std::string> new_line = {"", ""};
		std::vector<std::vector<std::string>> options = {
			{"Usage: " + app_name + " [OPTION]...\n" + "Log-like structure benchmark for append.", ""},
			new_line,
			{"--engine [name]", "engine name, possible values: " + available_engines()},
			{"--path [path]", "path to file"},
			{"--size [size]", "log size"},
			{"--element_count [count]", "number of elements to be inserted"},
			{"--element_size [size]", "number of bytes of each element"},
			{"--iterations [iterations]", "number of iterations. "},
			new_line,
			{"pmemstream related options:", ""},
			{"--block_size [size]", "block size"},
			{"--region_size [size]", "region size"},
			new_line,
			{"More iterations gives more robust statistical data, but takes more time", ""},
			{"--null_region_runtime", "indicates if **null** region runtime would be passed to append"},
			{"--help", "display this message"}};
		for (auto &option : options) {
			std::cout << std::setw(25) << std::left << option[0] << " " << option[1] << std::endl;
		}
	}
};
std::string config::app_name;
constexpr option config::long_options[];
constexpr std::array<std::string_view, 2> config::engine_names;

std::ostream &operator<<(std::ostream &out, config const &cfg)
{
	out << "Log-like structure Benchmark, path: " << cfg.path << ", ";
	out << "size: " << cfg.size << ", ";
	out << "block_size: " << cfg.block_size << ", ";
	out << "region_size: " << cfg.region_size << ", ";
	out << "element_count: " << cfg.element_count << ", ";
	out << "element_size: " << cfg.element_size << ", ";
	out << "null_region_runtime: " << std::boolalpha << cfg.null_region_runtime << ", ";
	out << "Number of iterations: " << cfg.iterations << std::endl;
	return out;
}

class workload_base {
 public:
	virtual ~workload_base(){};
	virtual void initialize() = 0;
	virtual void perform() = 0;
	virtual void clean() = 0;

	void prepare_data(size_t bytes_to_generate)
	{
		data = benchmark::generate_data(bytes_to_generate);
	}
	uint8_t *get_data_chunks()
	{
		return reinterpret_cast<uint8_t *>(data.data());
	}

 protected:
	std::vector<uint64_t> data;
};

class pmemlog_workload : public workload_base {
 public:
	pmemlog_workload(config &cfg)
	    : cfg(cfg), plp(pmemlog_create(cfg.path.c_str(), cfg.size, S_IRWXU), pmemlog_close)
	{
		if (plp.get() == nullptr) {
			plp.reset(pmemlog_open(cfg.path.c_str()));
		}
		if (plp.get() == nullptr) {
			throw std::runtime_error("Creating file: " + cfg.path +
						 " caused error: " + std::strerror(errno));
		}
	}

	void initialize() override
	{
		auto bytes_to_generate = cfg.element_count * cfg.element_size;
		prepare_data(bytes_to_generate);
	}

	void perform() override
	{
		auto data_chunks = get_data_chunks();
		for (size_t i = 0; i < data.size() * sizeof(uint64_t); i += cfg.element_size) {
			if (pmemlog_append(plp.get(), data_chunks + i, cfg.element_size) < 0) {
				throw std::runtime_error("Error while appending " + std::to_string(i) +
							 " entry! Errno: " + std::string(std::strerror(errno)));
			}
		}
	}

	void clean() override
	{
		pmemlog_rewind(plp.get());
	}

 private:
	config cfg;
	std::unique_ptr<PMEMlogpool, std::function<void(PMEMlogpool *)>> plp;
};

class pmemstream_workload : public workload_base {
 public:
	pmemstream_workload(config &cfg) : cfg(cfg)
	{
		stream = make_pmemstream(cfg.path.c_str(), cfg.block_size, cfg.size);
	}

	virtual void initialize() override
	{
		if (pmemstream_region_allocate(stream.get(), cfg.region_size, &region)) {
			throw std::runtime_error("Error during region allocate!");
		}
		if (!cfg.null_region_runtime &&
		    pmemstream_region_runtime_initialize(stream.get(), region, &region_runtime_ptr)) {
			throw std::runtime_error("Error during getting region runtime!");
		}

		auto bytes_to_generate = cfg.element_count * cfg.element_size;
		prepare_data(bytes_to_generate);
	}

	void perform() override
	{
		auto data_chunks = get_data_chunks();
		for (size_t i = 0; i < data.size() * sizeof(uint64_t); i += cfg.element_size) {
			if (pmemstream_append(stream.get(), region, region_runtime_ptr, data_chunks + i,
					      cfg.element_size, NULL) < 0) {
				throw std::runtime_error("Error while appending " + std::to_string(i) + " entry!");
			}
		}
	}

	void clean() override
	{
		pmemstream_region_free(stream.get(), region);
	}

 private:
	config cfg;
	struct pmemstream_region region;
	pmemstream_region_runtime *region_runtime_ptr;
	std::unique_ptr<struct pmemstream, std::function<void(struct pmemstream *)>> stream;
};

int main(int argc, char *argv[])
{
	// auto config = parse_arguments(argc, argv);
	config cfg;
	try {
		if (cfg.parse_arguments(argc, argv) != 0) {
			config::print_usage();
			exit(0);
		}
	} catch (std::invalid_argument const &e) {
		std::cerr << e.what() << std::endl;
		exit(1);
	}
	std::cout << cfg << std::endl;

	std::unique_ptr<workload_base> workload;

	if (cfg.engine == "pmemlog") {
		workload = std::make_unique<pmemlog_workload>(cfg);
	} else if (cfg.engine == "pmemstream") {
		workload = std::make_unique<pmemstream_workload>(cfg);
	}

	/* XXX: Add initialization phase whith separate measurement */
	std::vector<std::chrono::nanoseconds::rep> results;
	try {
		results = benchmark::measure<std::chrono::nanoseconds>(
			cfg.iterations, [&] { workload->initialize(); },
			[interface = workload.get()] { interface->perform(); }, [&] { workload->clean(); });
	} catch (std::runtime_error &e) {
		std::cerr << e.what() << std::endl;
		return -2;
	}

	auto mean = benchmark::mean(results) / cfg.element_count;
	auto max = static_cast<size_t>(benchmark::max(results)) / cfg.element_count;
	auto min = static_cast<size_t>(benchmark::min(results)) / cfg.element_count;
	auto std_dev = benchmark::std_dev(results) / cfg.element_count;

	std::cout << cfg.engine << " measurement:" << std::endl;
	std::cout << "\tmean[ns]: " << mean << std::endl;
	std::cout << "\tmax[ns]: " << max << std::endl;
	std::cout << "\tmin[ns]: " << min << std::endl;
	std::cout << "\tstandard deviation[ns]: " << std_dev << std::endl;
}
