// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

/* region_allocator.cpp -- tests region allocator */

#include <iostream>

int main(int argc, char *argv[])
{
	if (argc != 2) {
		std::cout << "Usage: " << argv[0] << " file-path" << std::endl;
		return -1;
	}
	return 0;
}