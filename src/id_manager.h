// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

/* Internal Header */

#ifndef LIBPMEMSTREAM_ID_MANAGER_H
#define LIBPMEMSTREAM_ID_MANAGER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* id_manager assign unique ids to clients which call id_manager_acquire.
 * Ids can be returned to id_manager (so that future clients can reuse them).
 *
 * Id returned to client is always lowest possible (starting from 0).
 */
struct id_manager;

struct id_manager *id_manager_new(void);
void id_manager_destroy(struct id_manager *manager);

/* Acquires unique id with the lowest possible value. */
uint64_t id_manager_acquire(struct id_manager *manager);

/* Release id so that it can be reused by subsequent id_manager_acquire. */
int id_manager_release(struct id_manager *manager, uint64_t id);

#ifdef __cplusplus
} /* end extern "C" */
#endif
#endif /* LIBPMEMSTREAM_ID_MANAGER_H */
