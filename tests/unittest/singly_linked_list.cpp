// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include "singly_linked_list.h"
#include "unittest.hpp"

#include <rapidcheck.h>

#include <cstring>

struct node {
	uint64_t data;
	uint64_t next;
};

std::ostream &operator<<(std::ostream &os, const node &n)
{
	os << n.data << " " << n.next;
	return os;
}

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
	rc::check("Insert head", [](const std::vector<struct node> &data) {
		RC_PRE(data.size() > 0);

		singly_linked_list list;
		SLIST_INIT(&list);

		struct pmemstream_data_runtime runtime {
			.spans = (uint64_t *)data.data(), .memcpy = &memcpy_mock, .memset = &memset_mock,
			.flush = &flush_mock, .drain = &drain_mock, .persist = &persist_mock
		};

		uint64_t offset = 0;
		for (size_t i = 0; i < data.size(); ++i) {
			SLIST_INSERT_HEAD(struct node, &runtime, &list, offset, next);
			offset += sizeof(struct node);
		}

		uint64_t it = 0;
		auto rit = data.rbegin();
		SLIST_FOREACH(struct node, &runtime, &list, it, next)
		{
			RC_ASSERT(((node *)(pmemstream_offset_to_ptr(&runtime, it)))->data == rit->data);
			rit++;
		}
		RC_ASSERT(rit != data.rbegin());
	});

	rc::check("Push back", [](const std::vector<struct node> &data) {
		RC_PRE(data.size() > 0);

		singly_linked_list list;
		SLIST_INIT(&list);
		struct pmemstream_data_runtime runtime {
			.spans = (uint64_t *)data.data(), .memcpy = &memcpy_mock, .memset = &memset_mock,
			.flush = &flush_mock, .drain = &drain_mock, .persist = &persist_mock
		};

		uint64_t offset = 0;
		for (size_t i = 0; i < data.size(); ++i) {
			SLIST_INSERT_TAIL(struct node, &runtime, &list, offset, next);
			offset += sizeof(struct node);
		}

		uint64_t it = 0;
		auto v_it = data.begin();
		SLIST_FOREACH(struct node, &runtime, &list, it, next)
		{
			RC_ASSERT(((node *)(pmemstream_offset_to_ptr(&runtime, it)))->data == v_it->data);
			v_it++;
		}
		RC_ASSERT(v_it != data.begin());
	});

	rc::check("Remove head", [](const std::vector<struct node> &data) {
		singly_linked_list list;
		SLIST_INIT(&list);
		struct pmemstream_data_runtime runtime {
			.spans = (uint64_t *)data.data(), .memcpy = &memcpy_mock, .memset = &memset_mock,
			.flush = &flush_mock, .drain = &drain_mock, .persist = &persist_mock
		};

		uint64_t offset = 0;
		for (size_t i = 0; i < data.size(); ++i) {
			SLIST_INSERT_TAIL(struct node, &runtime, &list, offset, next);
			offset += sizeof(struct node);
		}

		std::vector<struct node> mod_data = data;
		std::vector<struct node>::iterator it = mod_data.begin();
		if (it != mod_data.end())
			it = mod_data.erase(it);
		if (list.head != UINT64_MAX) {
			SLIST_REMOVE_HEAD(struct node, &runtime, &list, next);
		}

		uint64_t l_it = list.head;
		if (l_it == UINT64_MAX)
			return;

		auto v_it = mod_data.begin();
		while (v_it != mod_data.end()) {
			RC_ASSERT(((node *)(pmemstream_offset_to_ptr(&runtime, l_it)))->data == v_it->data);
			l_it = ((node *)(pmemstream_offset_to_ptr(&runtime, l_it)))->next;
			v_it++;
		}
	});

	rc::check("Random remove", [](const std::vector<struct node> &data) {
		RC_PRE(data.size() > 1);

		singly_linked_list list;
		SLIST_INIT(&list);
		struct pmemstream_data_runtime runtime {
			.spans = (uint64_t *)data.data(), .memcpy = &memcpy_mock, .memset = &memset_mock,
			.flush = &flush_mock, .drain = &drain_mock, .persist = &persist_mock
		};

		uint64_t offset = 0;
		for (size_t i = 0; i < data.size(); ++i) {
			SLIST_INSERT_TAIL(struct node, &runtime, &list, offset, next);
			offset += sizeof(struct node);
		}

		std::vector<struct node> mod_data(data);

		auto random_item_pos = *rc::gen::inRange<size_t>(1, data.size());
		std::vector<struct node>::iterator r_it = mod_data.begin();
		auto l_it = list.head;
		size_t i;
		for (i = 0; i < random_item_pos; ++i) {
			r_it++;
			l_it = ((node *)(pmemstream_offset_to_ptr(&runtime, l_it)))->next;
			RC_ASSERT(((node *)(pmemstream_offset_to_ptr(&runtime, l_it)))->data == r_it->data);
		}

		mod_data.erase(r_it);
		SLIST_REMOVE(struct node, &runtime, &list, l_it, next);

		l_it = list.head;
		auto v_it = mod_data.begin();
		SLIST_FOREACH(struct node, &runtime, &list, l_it, next)
		{
			RC_ASSERT(((node *)(pmemstream_offset_to_ptr(&runtime, l_it)))->data == v_it->data);
			v_it++;
		}
	});
}
