// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#ifndef LIBPMEMSTREAM_TEST_SIGHANDLERS_H
#define LIBPMEMSTREAM_TEST_SIGHANDLERS_H

#include <signal.h>

#include "test_backtrace.h"

/*
 * test_register_sighandlers -- register signal handlers for various fatal
 * signals
 */
static inline void test_register_sighandlers(void (*sighandler)(int))
{
#ifndef PMEMSTREAM_USE_TSAN
	signal(SIGSEGV, sighandler);
	signal(SIGABRT, sighandler);
	signal(SIGILL, sighandler);
	signal(SIGFPE, sighandler);
	signal(SIGINT, sighandler);
#ifndef _WIN32
	signal(SIGALRM, sighandler);
	signal(SIGQUIT, sighandler);
	signal(SIGBUS, sighandler);
#endif
#endif
}

#endif // LIBPMEMSTREAM_TEST_SIGHANDLERS_H
