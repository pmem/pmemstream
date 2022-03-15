// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include "singly_linked_list.h"
#include "unittest.hpp"

#include <cstring>

struct node {
	uint64_t data;
	uint64_t next;
};

int main(int argc, char *argv[])
{
	uint64_t data[1000];
	struct pmemstream_data_runtime runtime {
		.spans = data, .memcpy = NULL, .memset = NULL, .flush = NULL, .drain = NULL, .persist = NULL
	};

	singly_linked_list list;
	SLIST_INIT(&list);

	struct node node[3];
	for (int i = 0; i < 3; i++) {
		node[i].data = i;
	}

	memcpy(data, node, sizeof(struct node) * 3);

	SLIST_INSERT_HEAD(&runtime, &list, 0, struct node, next);
	SLIST_INSERT_HEAD(&runtime, &list, sizeof(struct node), struct node, next);
	SLIST_INSERT_HEAD(&runtime, &list, 2 * sizeof(struct node), struct node, next);

	uint64_t it = 0;
	SLIST_FOREACH(it, &runtime, &list, struct node, next)
	{
		std::cout << ((struct node *)pmemstream_offset_to_ptr(&runtime, it))->data << std::endl;
	}

    std::cout << list.head << std::endl;
    std::cout << list.tail << std::endl;
}

// // SPDX-License-Identifier: BSD-3-Clause
// /* Copyright 2021-2022, Intel Corporation */

// #include "singly_linked_list.h"
// #include "unittest.hpp"

// #include <rapidcheck.h>

// #include <cstring>

// struct node {
// 	uint64_t data;
// 	uint64_t next;
// };

// std::ostream &operator<<(std::ostream &os, const node &n)
// {
// 	os << n.data << " " << n.next;
// 	return os;
// }

// namespace rc
// {
// template <>
// struct Arbitrary<node> {
// 	static Gen<node> arbitrary()
// 	{
// 		return gen::construct<node>(gen::arbitrary<uint64_t>(), gen::just(UINT64_MAX));
// 	}
// };
// } // namespace rc

// int main(int argc, char *argv[])
// {
// 	rc::check("XXX", [](const std::vector<struct node> &data) {
// 		singly_linked_list list;
// 		SLIST_INIT(&list);

// 		struct pmemstream_data_runtime runtime {
// 			.spans = (uint64_t *)data.data(), .memcpy = NULL, .memset = NULL, .flush = NULL, .drain = NULL,
// 			.persist = NULL
// 		};

// 		uint64_t offset = 0;
// 		for (auto &d : data) {
// 			SLIST_INSERT_HEAD(&runtime, &list, offset, struct node, next);
// 			offset += sizeof(struct node);
// 		}

// 		uint64_t it = 0;
// 		uint64_t count = 0;
// 		SLIST_FOREACH(it, &runtime, &list, struct node, next)
// 		{
// 			RC_ASSERT(((struct node *)pmemstream_offset_to_ptr(&runtime, it))->data ==
// 				  data[data.size() - 1 - count].data);
// 		}
// 	});
// }

