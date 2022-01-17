// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

/* Internal Header */

#ifndef LIBPMEMSTREAM_THREAD_ID_H
#define LIBPMEMSTREAM_THREAD_ID_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct thread_id;

struct thread_id *thread_id_new(void);
void thread_id_destroy(struct thread_id *thread_id);

/*
 * Get this thread id. Value returned by this function will not change for the entire
 * time this thread lives. Ids returned are lowest possible (starting from 0).
 * Once thread finished, it's id can be reused by different threads.
 */
uint64_t thread_id_get(struct thread_id *thread_id);

#ifdef __cplusplus
} /* end extern "C" */
#endif
#endif /* LIBPMEMSTREAM_THREAD_ID_H */
