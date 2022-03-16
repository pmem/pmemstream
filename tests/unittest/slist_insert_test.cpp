// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include "singly_linked_list.h"
#include "unittest.hpp"

#include <rapidcheck.h>

struct node {
	uint64_t data;
	uint64_t next;
};

namespace rc
{
template <>
struct Arbitrary<node> {
	static Gen<node> arbitrary()
	{
		return gen::construct<node>(gen::arbitrary<uint64_t>(), gen::arbitrary<uint64_t>());
	}
};
} // namespace rc

void *memcpy_mock(void *dest, const void *src, size_t len, unsigned flags)
{
	return NULL;
}

void *memset_mock(void *dest, int c, size_t len, unsigned flags)
{
	return NULL;
}

void flush_mock(const void *ptr, size_t size)
{
	return;
}

void persist_mock(const void *ptr, size_t size)
{
	return;
}

void drain_mock(void)
{
	return;
}

int main(int argc, char *argv[])
{
	return run_test([] {
		rc::check("Insert head", [](const std::vector<struct node> &data) {
			singly_linked_list list;
			SLIST_INIT(&list);

			struct pmemstream_data_runtime runtime {
				.spans = (uint64_t *)data.data(), .memcpy = &memcpy_mock, .memset = &memset_mock,
				.flush = &flush_mock, .drain = &drain_mock, .persist = &persist_mock
			};

			SLIST_INSERT_HEAD(struct node, &runtime, &list, 0, next);

			auto rit = data.rbegin();
			RC_ASSERT(((node *)(pmemstream_offset_to_ptr(&runtime, 0)))->data == rit->data);
		});

		rc::check("Insert tail", [](const std::vector<struct node> &data) {
			singly_linked_list list;
			SLIST_INIT(&list);

			struct pmemstream_data_runtime runtime {
				.spans = (uint64_t *)data.data(), .memcpy = &memcpy_mock, .memset = &memset_mock,
				.flush = &flush_mock, .drain = &drain_mock, .persist = &persist_mock
			};

			SLIST_INSERT_TAIL(struct node, &runtime, &list, 0, next);

			auto rit = data.begin();
			RC_ASSERT(((node *)(pmemstream_offset_to_ptr(&runtime, 0)))->data == rit->data);
		});
	});
}
