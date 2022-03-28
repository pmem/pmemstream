// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include "singly_linked_list.h"
#include "stream_helpers.hpp"
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

int main(int argc, char *argv[])
{
	rc::check("Insert head", [](const std::vector<struct node> &data) {
		RC_PRE(data.size() > 0);

		singly_linked_list list;
		SLIST_INIT(&list);

		struct pmemstream_runtime runtime {
			.base = (void *)data.data(), .memcpy = &memcpy_mock, .memset = &memset_mock,
			.flush = &flush_mock, .drain = &drain_mock, .persist = &persist_mock
		};

		/* Add elements at the front of list */
		uint64_t offset = 0;
		for (size_t i = 0; i < data.size(); ++i) {
			SLIST_INSERT_HEAD(struct node, &runtime, &list, offset, next);
			offset += sizeof(struct node);
		}

		/* Check correctness */
		uint64_t it = 0;
		auto rit = data.rbegin();
		SLIST_FOREACH(struct node, &runtime, &list, it, next)
		{
			RC_ASSERT((SLIST_GET_PTR(node, &runtime, it))->data == rit->data);
			rit++;
		}
		RC_ASSERT(rit == data.rend());
	});

	rc::check("Push back", [](const std::vector<struct node> &data) {
		RC_PRE(data.size() > 0);

		singly_linked_list list;
		SLIST_INIT(&list);
		struct pmemstream_runtime runtime {
			.base = (void *)data.data(), .memcpy = &memcpy_mock, .memset = &memset_mock,
			.flush = &flush_mock, .drain = &drain_mock, .persist = &persist_mock
		};

		/* Add elements to list */
		uint64_t offset = 0;
		for (size_t i = 0; i < data.size(); ++i) {
			SLIST_INSERT_TAIL(struct node, &runtime, &list, offset, next);
			offset += sizeof(struct node);
		}

		/* Check correctness */
		uint64_t it = 0;
		auto v_it = data.begin();
		SLIST_FOREACH(struct node, &runtime, &list, it, next)
		{
			RC_ASSERT((SLIST_GET_PTR(node, &runtime, it))->data == v_it->data);
			v_it++;
		}
		RC_ASSERT(v_it == data.end());
	});

	rc::check("Remove head", [](const std::vector<struct node> &data) {
		RC_PRE(data.size() > 0);

		singly_linked_list list;
		SLIST_INIT(&list);
		struct pmemstream_runtime runtime {
			.base = (void *)data.data(), .memcpy = &memcpy_mock, .memset = &memset_mock,
			.flush = &flush_mock, .drain = &drain_mock, .persist = &persist_mock
		};

		/* Add elements to list */
		uint64_t offset = 0;
		for (size_t i = 0; i < data.size(); ++i) {
			SLIST_INSERT_TAIL(struct node, &runtime, &list, offset, next);
			offset += sizeof(struct node);
		}

		/* Remove head */
		auto mod_data = data;
		auto it = mod_data.begin();
		if (it != mod_data.end())
			it = mod_data.erase(it);
		if (list.head != SLIST_INVALID_OFFSET) {
			SLIST_REMOVE_HEAD(struct node, &runtime, &list, next);
		}

		uint64_t l_it = list.head;
		if (l_it == SLIST_INVALID_OFFSET)
			return;

		/* Check correctness */
		it = mod_data.begin();
		while (it != mod_data.end()) {
			RC_ASSERT((SLIST_GET_PTR(node, &runtime, l_it))->data == it->data);
			l_it = SLIST_NEXT(struct node, &runtime, l_it, next);
			it++;
		}
		RC_ASSERT(it == mod_data.end());
	});

	rc::check("Random remove", [](const std::vector<struct node> &data) {
		RC_PRE(data.size() > 1);

		singly_linked_list list;
		SLIST_INIT(&list);
		struct pmemstream_runtime runtime {
			.base = (void *)data.data(), .memcpy = &memcpy_mock, .memset = &memset_mock,
			.flush = &flush_mock, .drain = &drain_mock, .persist = &persist_mock
		};

		/* Add elements to list */
		uint64_t offset = 0;
		for (size_t i = 0; i < data.size(); ++i) {
			SLIST_INSERT_TAIL(struct node, &runtime, &list, offset, next);
			offset += sizeof(struct node);
		}

		/* Remove random element from both lists */
		auto random_item_pos = *rc::gen::inRange<size_t>(1, data.size());
		auto mod_data(data);
		auto r_it = mod_data.begin();
		auto l_it = list.head;

		for (size_t i = 0; i < random_item_pos; ++i) {
			r_it++;
			l_it = SLIST_NEXT(struct node, &runtime, l_it, next);
			RC_ASSERT((SLIST_GET_PTR(node, &runtime, l_it))->data == r_it->data);
		}

		mod_data.erase(r_it);
		SLIST_REMOVE(struct node, &runtime, &list, l_it, next);

		/* Check correctness */
		l_it = list.head;
		auto v_it = mod_data.begin();
		SLIST_FOREACH(struct node, &runtime, &list, l_it, next)
		{
			RC_ASSERT((SLIST_GET_PTR(struct node, &runtime, l_it))->data == v_it->data);
			v_it++;
		}
		RC_ASSERT(v_it == mod_data.end());
		RC_ASSERT(mod_data.size() == data.size() - 1);
	});
}
