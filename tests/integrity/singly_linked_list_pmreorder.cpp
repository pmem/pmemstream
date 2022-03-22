// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include "pmemstream_runtime.h"
#include "singly_linked_list.h"
#include "unittest.hpp"
#include <filesystem>
#include <iostream>

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <random>
#include <vector>

static std::mt19937_64 rnd_generator;

void init_random()
{
	uint64_t seed;
	const char *seed_env = std::getenv("TEST_SEED");
	if (seed_env == NULL) {
		std::random_device rd;
		seed = rd();
		std::cout << "To reproduce set env variable TEST_SEED=" << seed << std::endl;
	} else {
		seed = std::stoull(seed_env);
		std::cout << "Running with TEST_SEED=" << seed << std::endl;
	}
	rnd_generator = std::mt19937_64(seed);
}

struct node {
	uint64_t next = 0xDEAD;
};

static auto make_pmem2_map = make_instance_ctor(map_open, map_delete);

pmemstream_runtime get_runtime(pmem2_map *map)
{
	struct pmemstream_runtime runtime {
		.base = (uint64_t *)(pmem2_map_get_address(map)) + sizeof(singly_linked_list), .memcpy = NULL,
		.memset = NULL, .flush = pmem2_get_flush_fn(map), .drain = pmem2_get_drain_fn(map), .persist = NULL
	};

	return runtime;
}

singly_linked_list *get_list(pmem2_map *map)
{
	return static_cast<singly_linked_list *>(pmem2_map_get_address(map));
}

void slist_runtime_init(pmemstream_runtime *runtime, singly_linked_list *list)
{
	SLIST_RUNTIME_INIT(struct node, runtime, list, next);
}

void slist_insert_head(pmemstream_runtime *runtime, singly_linked_list *list, uint64_t offset)
{
	SLIST_INSERT_HEAD(struct node, runtime, list, offset, next);
}

void slist_insert_tail(pmemstream_runtime *runtime, singly_linked_list *list, uint64_t offset)
{
	SLIST_INSERT_TAIL(struct node, runtime, list, offset, next);
}

void slist_remove_head(pmemstream_runtime *runtime, singly_linked_list *list, uint64_t)
{
	SLIST_REMOVE_HEAD(struct node, runtime, list, next);
}

template <typename UnaryFunction>
void slist_foreach(pmemstream_runtime *runtime, singly_linked_list *list, UnaryFunction f)
{

	uint64_t it = 0;
	SLIST_FOREACH(struct node, runtime, list, it, next)
	{
		f(it);
	}
}

using slist_macro_wrapper = std::function<void(pmemstream_runtime *, singly_linked_list *, uint64_t)>;

std::vector<slist_macro_wrapper> generate_commands(size_t number_of_commands)
{
	static std::vector<slist_macro_wrapper> possible_cmds{slist_insert_head, slist_insert_tail, slist_remove_head};
	std::vector<slist_macro_wrapper> out;
	for (size_t i = 0; i < number_of_commands; i++) {
		const size_t samples_number = 1;
		std::sample(possible_cmds.begin(), possible_cmds.end(), std::back_inserter(out), samples_number,
			    rnd_generator);
	}
	return out;
}

template <typename OffsetOf>
std::vector<size_t> generate_offsets(size_t number_of_values)
{
	std::vector<size_t> numbers;
	std::vector<size_t> mixed_numbers;

	size_t offset = 0;
	for (size_t i = 0; i < number_of_values; i++) {
		numbers.push_back(offset);
		offset += sizeof(OffsetOf);
	}

	std::sample(numbers.begin(), numbers.end(), std::back_inserter(mixed_numbers), numbers.size(), rnd_generator);
	return mixed_numbers;
}

void slist_init(pmemstream_runtime *runtime, singly_linked_list *list)
{
	SLIST_INIT(runtime, list);
}

void create(std::filesystem::path path)
{
	constexpr bool truncate = true;
	auto map = make_pmem2_map(path.c_str(), TEST_DEFAULT_STREAM_SIZE, truncate);

	auto runtime = get_runtime(map.get());
	auto *list = get_list(map.get());

	slist_init(&runtime, list);
}

void fill(std::filesystem::path path)
{
	constexpr bool truncate = false;
	auto map = make_pmem2_map(path.c_str(), TEST_DEFAULT_STREAM_SIZE, truncate);
	auto runtime = get_runtime(map.get());
	auto *list = get_list(map.get());

	slist_runtime_init(&runtime, list);
	const size_t number_of_commands = 100;

	const auto commands = generate_commands(number_of_commands);
	const auto offsets = generate_offsets<node>(number_of_commands);

	for (size_t i = 0; i < number_of_commands; i++) {
		commands[i](&runtime, list, offsets[i]);
	}
}

std::filesystem::path make_working_copy(std::filesystem::path path)
{
	auto copy_path = path;
	copy_path += ".cpy";
	std::filesystem::copy_file(path, copy_path, std::filesystem::copy_options::overwrite_existing);
	return copy_path;
}

void check_consistency(std::filesystem::path path, bool with_recovery = true)
{
	auto copy_path = make_working_copy(path);
	constexpr bool truncate = false;
	auto map = make_pmem2_map(copy_path.c_str(), TEST_DEFAULT_STREAM_SIZE, truncate);
	auto runtime = get_runtime(map.get());
	singly_linked_list *list = get_list(map.get());

	if (with_recovery) {
		slist_runtime_init(&runtime, list);
	}

	uint64_t last_accessed = SLIST_INVALID_OFFSET;
	slist_foreach(&runtime, list, [&](uint64_t offset) { last_accessed = offset; });

	UT_ASSERTeq(last_accessed, list->tail);
}

int main(int argc, char *argv[])
{

	if (argc != 3)
		UT_FATAL("usage: %s <create|fill|check|check_without_recovery> file-name", argv[0]);

	std::string mode = argv[1];
	std::filesystem::path path(argv[2]);

	if (mode == "create") {
		create(path);
	} else if (mode == "fill") {
		init_random();
		fill(path);
	} else if (mode == "check") {
		check_consistency(path);
	} else if (mode == "check_without_recovery") {
		constexpr bool with_recovery = false;
		check_consistency(path, with_recovery);
	}
}
