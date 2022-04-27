// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include "singly_linked_list.h"
#include "stream_helpers.hpp"
#include "unittest.hpp"

#include <rapidcheck.h>

#include <cstring>

#define TEST_SLIST_DATA_BASE (1024UL)

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

template <typename T, class TIterator>
void check_list(pmemstream_runtime &runtime, singly_linked_list &list, TIterator begin, TIterator end)
{
	uint64_t it = 0;
	auto rit = begin;
	SLIST_FOREACH(T, &runtime, &list, it, next)
	{
		RC_ASSERT((SLIST_GET_PTR(T, &runtime, it))->data == rit->data);
		rit++;
	}
	RC_ASSERT(rit == end);
	RC_ASSERT(SLIST_INVARIANTS(T, &runtime, &list, next));
}

int main(int argc, char *argv[])
{

	return run_test([] {
		return_check ret;

		/* Initiate list */
		{
			singly_linked_list list;

			struct pmemstream_runtime runtime {
				.base = (void *)TEST_SLIST_DATA_BASE, .memcpy = &memcpy_mock, .memset = &memset_mock,
				.flush = &flush_mock, .drain = &drain_mock, .persist = &persist_mock
			};

			SLIST_INIT(&runtime, &list);

			UT_ASSERTeq(list.head, SLIST_INVALID_OFFSET);
			UT_ASSERTeq(list.tail, SLIST_INVALID_OFFSET);
		}

		ret += rc::check(
			"Push front and check if head points to the last inserted element",
			[](const std::vector<struct node> &data) {
				RC_PRE(data.size() > 0);

				singly_linked_list list;

				struct pmemstream_runtime runtime {
					.base = (void *)data.data(), .memcpy = &memcpy_mock, .memset = &memset_mock,
					.flush = &flush_mock, .drain = &drain_mock, .persist = &persist_mock
				};

				SLIST_INIT(&runtime, &list);

				/* Add elements at the front of list */
				uint64_t offset = 0;
				auto v_it = data.begin();
				for (size_t i = 0; i < data.size(); ++i) {
					SLIST_INSERT_HEAD(struct node, &runtime, &list, offset, next);

					RC_ASSERT((SLIST_GET_PTR(node, &runtime, list.head))->data == v_it->data);
					offset += sizeof(struct node);
					v_it++;
				}

				/* Check correctness */
				check_list<node>(runtime, list, data.rbegin(), data.rend());
			});

		ret += rc::check(
			"Push back and check if tail points to the last inserted",
			[](const std::vector<struct node> &data) {
				RC_PRE(data.size() > 0);

				singly_linked_list list;

				struct pmemstream_runtime runtime {
					.base = (void *)data.data(), .memcpy = &memcpy_mock, .memset = &memset_mock,
					.flush = &flush_mock, .drain = &drain_mock, .persist = &persist_mock
				};

				SLIST_INIT(&runtime, &list);

				/* Add elements to list */
				uint64_t offset = 0;
				auto v_it = data.begin();
				for (size_t i = 0; i < data.size(); ++i) {
					SLIST_INSERT_TAIL(struct node, &runtime, &list, offset, next);

					RC_ASSERT((SLIST_GET_PTR(node, &runtime, list.tail))->data == v_it->data);
					offset += sizeof(struct node);
					v_it++;
				}

				/* Check correctness */
				check_list<node>(runtime, list, data.begin(), data.end());
			});

		ret += rc::check("Remove head", [](const std::vector<struct node> &data) {
			RC_PRE(data.size() > 0);

			singly_linked_list list;

			struct pmemstream_runtime runtime {
				.base = (void *)data.data(), .memcpy = &memcpy_mock, .memset = &memset_mock,
				.flush = &flush_mock, .drain = &drain_mock, .persist = &persist_mock
			};

			SLIST_INIT(&runtime, &list);

			/* Add elements to list */
			uint64_t offset = 0;
			for (size_t i = 0; i < data.size(); ++i) {
				SLIST_INSERT_TAIL(struct node, &runtime, &list, offset, next);
				offset += sizeof(struct node);
			}

			/* Number of elements to remove */
			auto num_to_rmv = *rc::gen::inRange<size_t>(1, data.size() + 1);
			auto mod_data = data;

			for (size_t i = 1; i <= num_to_rmv; ++i) {
				/* Remove head */
				mod_data.erase(mod_data.begin());
				SLIST_REMOVE_HEAD(struct node, &runtime, &list, next);

				/* Check correctness */
				check_list<node>(runtime, list, mod_data.begin(), mod_data.end());
				RC_ASSERT(mod_data.size() == data.size() - i);
			}
		});

		ret += rc::check("Random remove", [](const std::vector<struct node> &data) {
			RC_PRE(data.size() > 0);

			singly_linked_list list;

			struct pmemstream_runtime runtime {
				.base = (void *)data.data(), .memcpy = &memcpy_mock, .memset = &memset_mock,
				.flush = &flush_mock, .drain = &drain_mock, .persist = &persist_mock
			};

			SLIST_INIT(&runtime, &list);

			/* Add elements to list */
			uint64_t offset = 0;
			for (size_t i = 0; i < data.size(); ++i) {
				SLIST_INSERT_TAIL(struct node, &runtime, &list, offset, next);
				offset += sizeof(struct node);
			}

			/* Number of elements to remove */
			auto num_to_rmv = *rc::gen::inRange<size_t>(1, data.size() + 1);
			auto mod_data(data);

			for (size_t i = 1; i <= num_to_rmv; ++i) {
				/* Remove random element from the list */
				auto random_item_pos = *rc::gen::inRange<size_t>(0, mod_data.size());

				auto r_it = mod_data.begin();
				std::advance(r_it, random_item_pos);
				auto l_it = list.head;

				for (size_t i = 0; i < random_item_pos; ++i) {
					l_it = SLIST_NEXT(struct node, &runtime, l_it, next);
				}

				RC_ASSERT((SLIST_GET_PTR(node, &runtime, l_it))->data == r_it->data);
				mod_data.erase(r_it);
				SLIST_REMOVE(struct node, &runtime, &list, l_it, next);

				/* Check correctness */
				check_list<node>(runtime, list, mod_data.begin(), mod_data.end());
				RC_ASSERT(mod_data.size() == data.size() - i);
			}
		});

		rc::check("Removing nonexistent element doesn't change the list",
			  [](const std::vector<struct node> &data) {
				  RC_PRE(data.size() > 0);

				  singly_linked_list list;

				  struct pmemstream_runtime runtime {
					  .base = (void *)data.data(), .memcpy = &memcpy_mock, .memset = &memset_mock,
					  .flush = &flush_mock, .drain = &drain_mock, .persist = &persist_mock
				  };

				  SLIST_INIT(&runtime, &list);

				  /* Add elements at the front of list */
				  uint64_t offset = 0;
				  for (size_t i = 0; i < data.size(); ++i) {
					  SLIST_INSERT_HEAD(struct node, &runtime, &list, offset, next);
					  offset += sizeof(struct node);
				  }

				  auto invalid_offset = *rc::gen::inRange<uint64_t>(offset + 1, UINT64_MAX);
				  SLIST_REMOVE(struct node, &runtime, &list, invalid_offset, next);

				  /* Check correctness */
				  check_list<node>(runtime, list, data.rbegin(), data.rend());
			  });

		ret += rc::check("Re-insert elements to the list after they were removed",
				 [](const std::vector<struct node> &data, const std::vector<struct node> &extra_data,
				    bool is_empty) {
					 RC_PRE(data.size() > 0);
					 RC_PRE(extra_data.size() > 0);

					 singly_linked_list list;

					 auto data_rnt(data);

					 /* Add extra data */
					 if (!is_empty) {
						 data_rnt.insert(data_rnt.end(), extra_data.begin(), extra_data.end());
					 }

					 struct pmemstream_runtime runtime {
						 .base = (void *)data_rnt.data(), .memcpy = &memcpy_mock,
						 .memset = &memset_mock, .flush = &flush_mock, .drain = &drain_mock,
						 .persist = &persist_mock
					 };

					 SLIST_INIT(&runtime, &list);

					 /* Add elements to the list */
					 uint64_t offset = 0;
					 for (size_t i = 0; i < data_rnt.size(); ++i) {
						 SLIST_INSERT_TAIL(struct node, &runtime, &list, offset, next);
						 offset += sizeof(struct node);
					 }

					 /* Remove data from the list */
					 for (size_t i = 0; i < data.size(); ++i) {
						 SLIST_REMOVE_HEAD(struct node, &runtime, &list, next);
					 }

					 /* Insert removed data */
					 offset = sizeof(struct node) * (data.size() - 1);
					 for (size_t i = 0; i < data.size(); ++i) {
						 SLIST_INSERT_HEAD(struct node, &runtime, &list, offset, next);
						 offset -= sizeof(struct node);
					 }

					 /* Check correctness */
					 check_list<node>(runtime, list, data_rnt.begin(), data_rnt.end());
				 });
	});
}
