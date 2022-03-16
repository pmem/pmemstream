// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include "singly_linked_list.h"
#include "unittest.hpp"

int main(int argc, char *argv[])
{
	return run_test([] {
		singly_linked_list list;
		SLIST_INIT(&list);

		UT_ASSERTeq(list.head, UINT64_MAX);
		UT_ASSERTeq(list.tail, UINT64_MAX);
	});
}
