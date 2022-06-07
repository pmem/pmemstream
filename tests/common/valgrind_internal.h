// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#ifndef VALGRIND_INTERNAL_HPP
#define VALGRIND_INTERNAL_HPP

#ifdef __cplusplus
extern "C" {
#endif

extern unsigned On_valgrind;
extern unsigned On_pmemcheck;
extern unsigned On_memcheck;
extern unsigned On_helgrind;
extern unsigned On_drd;

void set_valgrind_internals(void);

#ifdef __cplusplus
}
#endif

#endif /* VALGRIND_INTERNAL_HPP */
