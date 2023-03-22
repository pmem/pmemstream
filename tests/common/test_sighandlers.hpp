// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#ifndef LIBPMEMSTREAM_TEST_SIGHANDLERS_HPP
#define LIBPMEMSTREAM_TEST_SIGHANDLERS_HPP

#include <csetjmp>
#include <cstring>
#include <stdexcept>
#include <string>

#include "test_sighandlers.h"

static inline sigjmp_buf &sigjmp_tls_buf()
{
	static thread_local sigjmp_buf sigjmp;
	return sigjmp;
}

static inline void test_handle_signal_for_debug(int sig)
{
	if (sig == SIGKILL) {
		test_backtrace_sighandler(sig);
	} else {
		siglongjmp(sigjmp_tls_buf(), sig);
	}
}

static inline int test_register_signal_handler_for_debug()
{
	int sig = sigsetjmp(sigjmp_tls_buf(), 0);
	if (sig != 0) {
		test_register_sighandlers(test_backtrace_sighandler);
		throw std::runtime_error("Signal: " + std::string(strsignal(sig)) + " received!");
	} else {
		test_register_sighandlers(test_handle_signal_for_debug);
		return 0;
	}
}

static inline int test_register_sighandlers()
{
	if (getenv("PMEMSTREAM_HANDLE_SIGNAL_FOR_DEBUG")) {
		return test_register_signal_handler_for_debug();
	} else {
		test_register_sighandlers(test_backtrace_sighandler);
		return 0;
	}
}

#endif // LIBPMEMSTREAM_TEST_SIGHANDLERS_HPP
