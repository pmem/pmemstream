// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include "pmemstream_runtime.h"
#include "singly_linked_list.h"
#include "unittest.hpp"
#include <filesystem>
#include <iostream>

#include <functional>
#include <rapidcheck.h>
#include <vector>

struct node {
	uint64_t next = 0xDEAD;
};

auto make_pmem2_map = make_instance_ctor(map_open, map_delete);

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
void foreach (pmemstream_runtime *runtime, singly_linked_list * list, UnaryFunction f)
{

	uint64_t it = 0;
	SLIST_FOREACH(struct node, runtime, list, it, next)
	{
		f(it);
	}
}

using callable = std::function<void(pmemstream_runtime *, singly_linked_list *, uint64_t)>;
struct funcWrapper {
	callable func;
	size_t offset;
};

namespace rc
{

template <>
struct Arbitrary<funcWrapper> {
	static Gen<funcWrapper> arbitrary()
	{

		static std::vector<callable> possible_values{slist_insert_head, slist_insert_tail, slist_remove_head};

		constexpr size_t test_memory_size = TEST_DEFAULT_STREAM_SIZE - sizeof(struct node);
		auto offset_predicate = [](size_t value) { return (value % sizeof(struct node)) == 0; };

		return gen::build<funcWrapper>(
			gen::set(&funcWrapper::func, gen::elementOf(possible_values)),
			gen::set(&funcWrapper::offset,
				 gen::suchThat(gen::inRange<size_t>(8, test_memory_size), offset_predicate)));
	}
};

} /* namespace rc */

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

	rc::check([&](const std::vector<funcWrapper> &functions) {
		for (auto &f : functions) {
			f.func(&runtime, list, f.offset);
		}
	});
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
	constexpr bool truncate = false;
	auto copy_path = make_working_copy(path);
	auto map = make_pmem2_map(copy_path.c_str(), TEST_DEFAULT_STREAM_SIZE, truncate);
	auto runtime = get_runtime(map.get());
	singly_linked_list *list = get_list(map.get());

	if (with_recovery) {
		slist_runtime_init(&runtime, list);
	}

	uint64_t last_accessed = SLIST_INVALID_OFFSET;
	foreach (&runtime, list, [&](uint64_t offset) { last_accessed = offset; })
		;

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
		fill(path);
	} else if (mode == "check") {
		check_consistency(path);
	} else if (mode == "check_without_recovery") {
		constexpr bool with_recovery = false;
		check_consistency(path, with_recovery);
	}
}
