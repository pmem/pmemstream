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
#include "stream_helpers.hpp"

#include <libminiasync.h>
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
						  {"concurrency", required_argument, NULL, 't'},
						  {"async_append", no_argument, NULL, 'a'},
						  {"committing_threads", required_argument, NULL, 'm'},
						  {"persisting_threads", required_argument, NULL, 'g'},
						  {"wait_period", required_argument, NULL, 'w'},
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
	size_t block_size = TEST_DEFAULT_BLOCK_SIZE;
	size_t region_size = TEST_DEFAULT_REGION_SIZE * 10;
	size_t element_count = 100;
	size_t element_size = 1024;
	size_t iterations = 10;
	bool null_region_runtime = false;
	size_t concurrency = 1;
	bool async_append = false;
	size_t committing_threads = 0;
	size_t persisting_threads = 0;
	size_t wait_period = 0;

	int parse_arguments(int argc, char *argv[])
	{
		app_name = std::string(argv[0]);
		int ch;
		while ((ch = getopt_long(argc, argv, "e:p:x:b:r:c:s:i:nt:am:g:w:h", long_options, NULL)) != -1) {
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
				case 't':
					concurrency = std::stoull(optarg);
					break;
				case 'a':
					async_append = true;
					break;
				case 'm':
					committing_threads = std::stoull(optarg);
					break;
				case 'g':
					persisting_threads = std::stoull(optarg);
					break;
				case 'w':
					wait_period = std::stoull(optarg);
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
		if (async_append && (!wait_period || !(committing_threads + persisting_threads))) {
			throw std::invalid_argument(
				"wait_period and commiting_threads or persisting_threads must not be 0 when async_append is used");
		}
		if (committing_threads + persisting_threads + wait_period && !async_append) {
			throw std::invalid_argument(
				"Commiting threads and persisting threads and wait_period can only be set for async appends");
		}
		if (committing_threads && persisting_threads) {
			throw std::invalid_argument("Only committing or persiting threads can be configured, not both");
		}
		if (committing_threads + persisting_threads > concurrency) {
			throw std::invalid_argument(
				"Number of commiting threads and persisting threads exceeds concurrency");
		}
		if (wait_period > element_count) {
			throw std::invalid_argument("wait_period must be less than or equal to element_count");
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
			{"--concurrency [num]", "number of threads, which append concurrently"},
			{"--async_append",
			 "perform appends asynchronously. If this flag is specified, it's also required to set wait_period and either committing_thread or persisting_thread (but not both)"},
			{"--committing_threads",
			 "number of threads performing commit operation. Can only be used for async appends. Exclusive with persisting_threads"},
			{"--persisting_threads",
			 "number of threads performing persist operation. Can only be used for async appends. Exclusive with commiting_threads"},
			{"--wait_period [num_ops]", "how many entries are written before commit/perist_wait is called"},
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
	out << "Number of iterations: " << cfg.iterations << ", ";
	out << "Async append: " << cfg.async_append << ", ";
	out << "Committing threads: " << cfg.committing_threads << ", ";
	out << "Persisting threads: " << cfg.persisting_threads << ", ";
	out << "Wait period: " << cfg.wait_period << std::endl;
	return out;
}

class pmemlog_workload : public benchmark::workload_base {
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

	void perform(size_t thread_id) override
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

class pmemstream_workload : public benchmark::workload_base {
 public:
	pmemstream_workload(config &cfg) : cfg(cfg)
	{
		stream = make_pmemstream(cfg.path.c_str(), cfg.block_size, cfg.size);
	}

	virtual void initialize() override
	{
		for (size_t i = 0; i < cfg.concurrency; i++) {
			regions.emplace_back(allocate_region());
		}

		auto bytes_to_generate = cfg.element_count * cfg.element_size;
		prepare_data(bytes_to_generate);
	}

	void perform(size_t thread_id) override
	{
		auto data_chunks = get_data_chunks();
		for (size_t i = 0; i < data.size() * sizeof(uint64_t); i += cfg.element_size) {
			if (pmemstream_append(stream.get(), regions[thread_id].region,
					      regions[thread_id].region_runtime, data_chunks + i, cfg.element_size,
					      NULL) < 0) {
				throw std::runtime_error("Error while appending " + std::to_string(i) +
							 " entry in thread " + std::to_string(thread_id) + "!");
			}
		}
	}

	void clean() override
	{
		for (size_t i = 0; i < cfg.concurrency; i++) {
			pmemstream_region_free(stream.get(), regions[i].region);
		}
		regions.clear();
	}

 protected:
	config cfg;
	std::unique_ptr<struct pmemstream, std::function<void(struct pmemstream *)>> stream;

	struct region_wrapper {
		pmemstream_region region;
		pmemstream_region_runtime *region_runtime;
	};

	std::vector<region_wrapper> regions;

	region_wrapper allocate_region()
	{
		pmemstream_region region = {0};
		pmemstream_region_runtime *region_runtime = nullptr;

		if (pmemstream_region_allocate(stream.get(), cfg.region_size, &region)) {
			throw std::runtime_error("Error during region allocate!");
		}
		if (!cfg.null_region_runtime &&
		    pmemstream_region_runtime_initialize(stream.get(), region, &region_runtime)) {
			throw std::runtime_error("Error during getting region runtime!");
		}

		return {region, region_runtime};
	}
};

class pmemstream_async_workload : public pmemstream_workload {
 public:
	pmemstream_async_workload(config &cfg)
	    : pmemstream_workload(cfg), dmv(data_mover_sync_new(), &data_mover_sync_delete)
	{
	}

	void perform(size_t thread_id) override
	{
		auto data_chunks = get_data_chunks();

		for (size_t i = 0; i < data.size() * sizeof(uint64_t); i += cfg.element_size) {
			struct pmemstream_entry entry;
			if (pmemstream_async_append(stream.get(), data_mover_sync_get_vdm(dmv.get()),
						    regions[thread_id].region, regions[thread_id].region_runtime,
						    data_chunks + i, cfg.element_size, &entry) < 0) {
				throw std::runtime_error("Error while appending " + std::to_string(i) +
							 " entry in thread " + std::to_string(thread_id) + "!");
			}

			if (thread_id < cfg.committing_threads && i % cfg.wait_period == 0) {
				auto future = pmemstream_async_wait_committed(
					stream.get(), pmemstream_entry_timestamp(stream.get(), entry));
				while (future_poll(FUTURE_AS_RUNNABLE(&future), NULL) != FUTURE_STATE_COMPLETE)
					;
			} else if (thread_id < cfg.persisting_threads && i % cfg.wait_period == 0) {
				auto future = pmemstream_async_wait_persisted(
					stream.get(), pmemstream_entry_timestamp(stream.get(), entry));
				while (future_poll(FUTURE_AS_RUNNABLE(&future), NULL) != FUTURE_STATE_COMPLETE)
					;
			}
		}
	}

 private:
	std::unique_ptr<data_mover_sync, decltype(&data_mover_sync_delete)> dmv;
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

	std::unique_ptr<benchmark::workload_base> workload;

	if (cfg.engine == "pmemlog") {
		workload = std::make_unique<pmemlog_workload>(cfg);
	} else if (cfg.engine == "pmemstream" && !cfg.async_append) {
		workload = std::make_unique<pmemstream_workload>(cfg);
	} else if (cfg.engine == "pmemstream") {
		workload = std::make_unique<pmemstream_async_workload>(cfg);
	}

	/* XXX: Add initialization phase with separate measurement */
	std::vector<std::chrono::nanoseconds::rep> results;
	try {
		results = benchmark::measure<std::chrono::nanoseconds>(cfg.iterations, workload.get(), cfg.concurrency);
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
