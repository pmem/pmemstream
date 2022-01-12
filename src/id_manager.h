// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

/* Internal Header */

#ifndef LIBPMEMSTREAM_ID_MANAGER_H
#define LIBPMEMSTREAM_ID_MANAGER_H

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

#include "critnib/critnib.h"

#ifdef __cplusplus
extern "C" {
#endif

struct id {
	uint64_t value;
};

struct id_manager {
	/* A set (key == value) of all available ids between 0 and next_id. */
	critnib *ids;

	/* Minimum value bigger than all ids which reside in ids container and all
	 * granted ids. */
	uint64_t next_id;

	/* Mutex for protecting acquiring/releasing id */
	pthread_mutex_t id_acquire_release_mutex;
};

struct id_manager *id_manager_new(void);
void id_manager_destroy(struct id_manager *manager);

/* Acquires unique id with the lowest possible value. */
uint64_t id_manager_acquire_id(struct id_manager *manager);

/* Release id so that it can be reused by subsequent id_manager_acquire_id. */
int id_manager_release_id(struct id_manager *manager, uint64_t id);

#ifdef __cplusplus
} /* end extern "C" */
#endif
#endif /* LIBPMEMSTREAM_ID_MANAGER_H */
