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

int main(int argc, char *argv[])
{
	rc::check("XXX", [](const std::vector<struct node> &data) {
		singly_linked_list list;
		SLIST_INIT(&list);

		struct pmemstream_data_runtime runtime {
			.spans = (uint64_t *)data.data(), .memcpy = NULL, .memset = NULL, .flush = NULL, .drain = NULL,
			.persist = NULL
		};

		uint64_t offset = 0;
		for (size_t i = 0; i < data.size(); ++i) {
			SLIST_INSERT_HEAD(&runtime, &list, offset, struct node, next);
			offset += sizeof(struct node);
		}

		uint64_t it = 0;
		auto rit = data.rbegin();
		SLIST_FOREACH(it, &runtime, &list, struct node, next)
		{
			RC_ASSERT(((node *)(pmemstream_offset_to_ptr(&runtime, it)))->data == rit->data);
			rit++;
		}
	});

	rc::check("Push back", [](const std::vector<struct node> &data) {
		singly_linked_list list;
		SLIST_INIT(&list);
		struct pmemstream_data_runtime runtime {
			.spans = (uint64_t *)data.data(), .memcpy = NULL, .memset = NULL, .flush = NULL, .drain = NULL,
			.persist = NULL
		};

		uint64_t offset = 0;
		for (size_t i = 0; i < data.size(); ++i) {
			SLIST_INSERT_TAIL(&runtime, &list, offset, struct node, next);
			offset += sizeof(struct node);
		}

		uint64_t it = 0;
		auto v_it = data.begin();
		SLIST_FOREACH(it, &runtime, &list, struct node, next)
		{
			RC_ASSERT(((node *)(pmemstream_offset_to_ptr(&runtime, it)))->data == v_it->data);
			v_it++;
		}
	});
}
