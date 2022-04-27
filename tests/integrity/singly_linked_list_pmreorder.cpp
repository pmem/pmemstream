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
#include <list>
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

template <typename UnaryFunction>
void slist_foreach(pmemstream_runtime *runtime, singly_linked_list *list, UnaryFunction f)
{
	uint64_t it = 0;
	SLIST_FOREACH(struct node, runtime, list, it, next)
	{
		f(it);
	}
}

void create(test_config_type test_config)
{
	constexpr bool truncate = true;
	auto map = make_pmem2_map(test_config.filename.c_str(), test_config.stream_size, truncate);

	auto runtime = get_runtime(map.get());
	auto *list = get_list(map.get());

	slist_init(&runtime, list);
}

template <typename T>
T get_one_of(const std::list<T> &elements)
{
	auto it = elements.begin();
	std::advance(it, rnd_generator() % elements.size());
	return *it;
}

class commands_generator {
 public:
	static std::vector<std::function<void()>> generate(size_t number_of_commands, pmemstream_runtime *rt,
							   singly_linked_list *list)
	{
		std::shared_ptr<state> st = std::make_shared<state>(number_of_commands);
		std::vector<std::function<void()>> commands;
		std::vector<std::function<void()>> funcs = {
			slist_insert_head(rt, list, st), slist_insert_tail(rt, list, st),
			slist_remove_head(rt, list, st), slist_remove(rt, list, st)};

		commands = generate_commands(number_of_commands, funcs);
		return commands;
	}

 private:
	class state {
	 public:
		state(size_t size) : possible_offsets(size), generated(0)
		{
			std::iota(possible_offsets.begin(), possible_offsets.end(), 0);
			for (auto &val : possible_offsets) {
				val = val * sizeof(node);
			}
			std::shuffle(possible_offsets.begin(), possible_offsets.end(), rnd_generator);
		}

		size_t generate_offset()
		{
			return possible_offsets[generated++];
		}

		std::list<size_t> used_offsets;

	 private:
		std::vector<size_t> possible_offsets;
		size_t generated;
	};

	static std::function<void()> slist_insert_head(pmemstream_runtime *runtime, singly_linked_list *list,
						       std::shared_ptr<state> st)
	{
		return [=]() {
			size_t offset = st->generate_offset();
			st->used_offsets.push_front(offset);
			SLIST_INSERT_HEAD(struct node, runtime, list, offset, next);
		};
	}

	static std::function<void()> slist_insert_tail(pmemstream_runtime *runtime, singly_linked_list *list,
						       std::shared_ptr<state> st)
	{
		return [=]() {
			size_t offset = st->generate_offset();
			st->used_offsets.push_back(offset);
			SLIST_INSERT_TAIL(struct node, runtime, list, offset, next);
		};
	}

	static std::function<void()> slist_remove_head(pmemstream_runtime *runtime, singly_linked_list *list,
						       std::shared_ptr<state> st)
	{
		return [=]() {
			if (!st->used_offsets.empty()) {
				st->used_offsets.pop_front();
			}
			SLIST_REMOVE_HEAD(struct node, runtime, list, next);
		};
	}

	static std::function<void()> slist_remove(pmemstream_runtime *runtime, singly_linked_list *list,
						  std::shared_ptr<state> st)
	{
		return [=]() {
			size_t offset_to_del;
			if (!st->used_offsets.empty()) {
				offset_to_del = get_one_of(st->used_offsets);
				st->used_offsets.erase(std::find(std::begin(st->used_offsets),
								 std::end(st->used_offsets), offset_to_del));
			} else {
				offset_to_del = st->generate_offset();
			}
			SLIST_REMOVE(struct node, runtime, list, offset_to_del, next);
		};
	}
};

void fill(test_config_type test_config)
{
	constexpr bool truncate = false;
	auto map = make_pmem2_map(test_config.filename.c_str(), test_config.stream_size, truncate);
	auto runtime = get_runtime(map.get());
	auto *list = get_list(map.get());

	slist_runtime_init(&runtime, list);

	auto commands = commands_generator::generate(number_of_commands, &runtime, list);
	for (auto &command : commands) {
		command();
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
		UT_FATAL("usage: %s <create|fill|check|check_no_recovery> file-path", argv[0]);

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
		} else if (mode == "check_no_recovery") {
			constexpr bool with_recovery = false;
			check_consistency(test_config, with_recovery);
		} else {
			UT_FATAL("Wrong mode given!\nUsage: %s <create|fill|check|check_no_recovery> file-path",
				 argv[0]);
		}
	});
}
