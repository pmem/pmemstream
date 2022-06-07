// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <stdlib.h>

#include "valgrind_internal.h"

unsigned On_valgrind = 0;
unsigned On_pmemcheck = 0;
unsigned On_memcheck = 0;
unsigned On_helgrind = 0;
unsigned On_drd = 0;

void set_valgrind_internals(void)
{
	if (getenv("PMEMSTREAM_TRACER_PMEMCHECK"))
		On_pmemcheck = 1;
	else if (getenv("PMEMSTREAM_TRACER_MEMCHECK"))
		On_memcheck = 1;
	else if (getenv("PMEMSTREAM_TRACER_HELGRIND"))
		On_helgrind = 1;
	else if (getenv("PMEMSTREAM_TRACER_DRD"))
		On_drd = 1;

	On_valgrind = On_pmemcheck || On_memcheck || On_helgrind || On_drd;
}
