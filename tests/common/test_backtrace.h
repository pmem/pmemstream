// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2021, Intel Corporation */

#ifndef LIBPMEMSTREAM_TEST_BACKTRACE_H
#define LIBPMEMSTREAM_TEST_BACKTRACE_H

#ifdef __cplusplus
extern "C" {
#endif

void test_dump_backtrace(void);
void test_sighandler(int sig);
void test_register_sighandlers(void);

#ifdef __cplusplus
}
#endif
#endif /* LIBPMEMSTREAM_TEST_BACKTRACE_H */
