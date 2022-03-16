// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include "singly_linked_list.h"
#include "stream_helpers.hpp"
#include "unittest.hpp"

#define TEST_SLIST_DATA_BASE (1024UL)

int main(int argc, char *argv[])
{
	return run_test([] {
		singly_linked_list list;

		struct pmemstream_runtime runtime {
			.base = (void *)TEST_SLIST_DATA_BASE, .memcpy = &memcpy_mock, .memset = &memset_mock,
			.flush = &flush_mock, .drain = &drain_mock, .persist = &persist_mock
		};

		SLIST_INIT(&runtime, &list);

		UT_ASSERTeq(list.head, SLIST_INVALID_OFFSET);
		UT_ASSERTeq(list.tail, SLIST_INVALID_OFFSET);
	});
}
