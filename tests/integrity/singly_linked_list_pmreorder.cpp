// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include "pmemstream_runtime.h"
#include "random_helpers.hpp"
#include "singly_linked_list.h"
#include "unittest.hpp"

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <vector>

static constexpr size_t number_of_commands = 100;

static auto make_pmem2_map = make_instance_ctor(map_open, map_delete);

struct node {
	uint64_t next = 0xDEAD;
};

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

void slist_init(pmemstream_runtime *runtime, singly_linked_list *list)
{
	SLIST_INIT(runtime, list);
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

template <typename Node>
std::vector<size_t> generate_offsets(size_t number_of_values)
{
	std::vector<size_t> offsets;

	size_t offset = 0;
	for (size_t i = 0; i < number_of_values; i++) {
		offsets.push_back(offset);
		offset += sizeof(Node);
	}

	std::shuffle(offsets.begin(), offsets.end(), rnd_generator);
	return offsets;
}

void create(test_config_type test_config)
{
	constexpr bool truncate = true;
	auto map = make_pmem2_map(test_config.filename.c_str(), test_config.stream_size, truncate);

	auto runtime = get_runtime(map.get());
	auto *list = get_list(map.get());

	slist_init(&runtime, list);
}

void fill(test_config_type test_config)
{
	constexpr bool truncate = false;
	auto map = make_pmem2_map(test_config.filename.c_str(), test_config.stream_size, truncate);
	auto runtime = get_runtime(map.get());
	auto *list = get_list(map.get());

	slist_runtime_init(&runtime, list);

	/* XXX: Add testing of remove */
	const auto commands = generate_commands<slist_macro_wrapper>(
		number_of_commands, {slist_insert_head, slist_insert_tail, slist_remove_head});
	const auto offsets = generate_offsets<node>(number_of_commands);

	for (size_t i = 0; i < number_of_commands; i++) {
		commands[i](&runtime, list, offsets[i]);
	}
}

void check_consistency(test_config_type test_config, bool with_recovery = true)
{
	auto copy_path = copy_file(test_config.filename);
	constexpr bool truncate = false;
	auto map = make_pmem2_map(copy_path.c_str(), test_config.stream_size, truncate);
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

	struct test_config_type test_config;
	std::string mode = argv[1];
	test_config.filename = std::string(argv[2]);

	return run_test(test_config, [&] {
		if (mode == "create") {
			create(test_config);
		} else if (mode == "fill") {
			init_random();
			fill(test_config);
		} else if (mode == "check") {
			check_consistency(test_config);
		} else if (mode == "check_without_recovery") {
			constexpr bool with_recovery = false;
			check_consistency(test_config, with_recovery);
		}
	});
}
