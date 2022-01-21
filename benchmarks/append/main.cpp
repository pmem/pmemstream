// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <forward_list>
#include <getopt.h>
#include <iomanip>
#include <iostream>
#include <set>
#include <stdexcept>
#include <vector>

#include "measure.hpp"
/* XXX: Change this header when make_pmemstream moved to public API */
#include "unittest.hpp"

#include <libpmemlog.h>

class Config {
 private:
	static const std::set<std::string> engine_names;
	static constexpr option long_options[] = {{"engine", required_argument, NULL, 'e'},
						  {"path", required_argument, NULL, 'p'},
						  {"log_size", required_argument, NULL, 'x'},
						  {"block_size", required_argument, NULL, 'b'},
						  {"region_size", required_argument, NULL, 'r'},
						  {"element_count", required_argument, NULL, 'c'},
						  {"element_size", required_argument, NULL, 's'},
						  {"iterations", required_argument, NULL, 'i'},
						  {"null_region_runtime", no_argument, NULL, 'n'},
						  {"help", no_argument, NULL, 'h'},
						  {NULL, 0, NULL, 0}};

	static std::string app_name;

 public:
	std::string engine = "pmemstream";
	std::string path;
	size_t log_size = PMEMLOG_MIN_POOL;
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
		while ((ch = getopt_long(argc, argv, "e:p:x:b:r:c:s:i:nh", long_options, NULL)) != -1) {
			switch (ch) {
				case 'e':
					engine = std::string(optarg);
					if (engine_names.find(engine) == engine_names.end()) {
						std::string possible_engines;
						for (auto &name : engine_names) {
							possible_engines += name + " ";
						}
						throw std::invalid_argument(
							std::string("Wrong engine name, possible: ") +
							possible_engines);
					}
					break;
				case 'p':
					path = std::string(optarg);
					break;
				case 'x':
					log_size = std::stoull(optarg);
					if (log_size < PMEMLOG_MIN_POOL) {
						throw std::invalid_argument(std::string("Invalid size, should be >=") +
									    std::to_string(PMEMLOG_MIN_POOL));
					}
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
			{"Usage: " + app_name + " [OPTION]...\n" + "Pmemstream benchmark for append.", ""},
			new_line,
			{"--engine [name]", "engine name, possible values: pmemstream pmemlog"},
			{"--path [path]", "path to file"},
			{"--log_size [size]", "log size"},
			{"--element_count [count]", "number of elements inserted into stream"},
			{"--element_size [size]", "number of bytes of each element inserted into stream"},
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
std::string Config::app_name;
constexpr option Config::long_options[];
const std::set<std::string> Config::engine_names = std::initializer_list<std::string>{"pmemlog", "pmemstream"};

std::ostream &operator<<(std::ostream &out, Config const &cfg)
{
	out << "Pmemstream Benchmark, path: " << cfg.path << ", ";
	out << "log_size: " << cfg.log_size << ", ";
	out << "block_size: " << cfg.block_size << ", ";
	out << "region_size: " << cfg.region_size << ", ";
	out << "element_count: " << cfg.element_count << ", ";
	out << "element_size: " << cfg.element_size << ", ";
	out << "null_region_runtime: " << std::boolalpha << cfg.null_region_runtime << ", ";
	out << "Number of iterations: " << cfg.iterations << std::endl;
	return out;
}

class IWorkload {
 public:
	virtual ~IWorkload(){};
	virtual void perform() = 0;
	virtual int initialize() = 0;
	void prepareData(size_t bytes_to_generate)
	{
		auto input_generation_time = benchmark::measure<std::chrono::seconds>(
			[&] { data = benchmark::generate_data(bytes_to_generate); });
		data_chunks = reinterpret_cast<uint8_t *>(data.data());
		std::cout << "input generation time: " << input_generation_time << "s" << std::endl;
	}
	uint8_t *data_chunks;
	std::vector<uint64_t> data;
};

class PmemlogWorkload : public IWorkload {
 private:
	Config config;
	PMEMlogpool *plp;

 public:
	PmemlogWorkload(Config &config) : config(config)
	{
	}

	virtual void perform() override
	{
		for (auto it = data.begin(); it != data.end(); std::advance(it, config.element_size)) {
			if (pmemlog_append(plp, &it, config.element_size) < 0) {
				throw std::runtime_error("Error while appending new entry!");
			}
		}
	}

	virtual int initialize() override
	{
		auto path = config.path.c_str();
		plp = pmemlog_create(path, config.log_size, 0666);
		if (plp == NULL)
			plp = pmemlog_open(path);
		if (plp == NULL) {
			perror(path);
			// maybe throw?
			return -1;
		}
		auto bytes_to_generate = config.element_count * config.element_size;
		prepareData(bytes_to_generate);
		return 0;
	}
};

class PmemstreamWorkload : public IWorkload {
 private:
	Config config;
	struct pmemstream *pstream;
	struct pmemstream_region region;
	pmemstream_region_runtime *region_runtime_ptr;
	std::unique_ptr<struct pmemstream, std::function<void(struct pmemstream *)>> stream;

 public:
	PmemstreamWorkload(Config &config) : config(config)
	{
		stream = make_pmemstream(config.path.c_str(), config.block_size, config.log_size);
		pstream = stream.get();
	}

	virtual int initialize() override
	{
		if (pmemstream_region_allocate(stream.get(), config.region_size, &region)) {
			return -1;
		}
		if (!config.null_region_runtime &&
		    pmemstream_get_region_runtime(stream.get(), region, &region_runtime_ptr)) {
			return -2;
		}

		auto bytes_to_generate = config.element_count * config.element_size;
		prepareData(bytes_to_generate);

		return 0;
	}

	void perform() override
	{
		for (size_t i = 0; i < data.size() * sizeof(uint64_t); i += config.element_size) {
			pmemstream_append(stream.get(), region, region_runtime_ptr, data_chunks + i,
					  config.element_size, NULL);
		}
	}
};

int main(int argc, char *argv[])
{
	// auto config = parse_arguments(argc, argv);
	Config config;
	try {
		if (config.parse_arguments(argc, argv) != 0) {
			Config::print_usage();
			exit(0);
		}
	} catch (std::invalid_argument const &e) {
		std::cerr << e.what() << std::endl;
		exit(1);
	}
	std::cout << config << std::endl;

	IWorkload *workload = nullptr;

	if (config.engine == "pmemlog") {
		workload = new PmemlogWorkload(config);
	} else if (config.engine == "pmemstream") {
		workload = new PmemstreamWorkload(config);
	}

	if (workload->initialize() != 0) {
		std::cerr << "Error during initialization..." << std::endl;
		exit(2);
	}
	/* XXX: Add initialization phase whith separate measurement */
	auto results = benchmark::measure<std::chrono::nanoseconds>(config.iterations, [&] { workload->perform(); });

	delete workload;

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
