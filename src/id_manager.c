// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include "id_manager.h"
#include "critnib/critnib.h"

#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>

struct id_manager {
	/* A set of all available ids between 0 and next_id.
	 * Values are always set to NULL. We are only interested in whether
	 * particular key exists or not. */
	critnib *ids;

	/* Minimum value bigger than all ids which reside in ids container and all
	 * granted ids. */
	uint64_t next_id;

	/* Mutex for protecting acquiring/releasing id. */
	pthread_mutex_t id_acquire_release_mutex;
};

struct id_manager *id_manager_new(void)
{
	struct id_manager *manager = malloc(sizeof(*manager));
	if (!manager) {
		return NULL;
	}

	manager->next_id = 0;

	int ret = pthread_mutex_init(&manager->id_acquire_release_mutex, NULL);
	if (ret) {
		goto err_pthread_mutex_init;
	}

	manager->ids = critnib_new();
	if (!manager->ids) {
		goto err_critnib_new;
	}

	return manager;

err_critnib_new:
	pthread_mutex_destroy(&manager->id_acquire_release_mutex);
err_pthread_mutex_init:
	free(manager);
	return NULL;
}

void id_manager_destroy(struct id_manager *manager)
{
	pthread_mutex_destroy(&manager->id_acquire_release_mutex);
	critnib_delete(manager->ids);
	free(manager);
}

static bool id_manager_critnib_get_le_key(critnib *ids, uint64_t key, uint64_t *next_key)
{
	void *value = NULL;
	int found = critnib_find(ids, key, FIND_LE, next_key, &value);
	assert(value == NULL);
	return found;
}

static bool id_manager_critnib_get_ge_key(critnib *ids, uint64_t key, uint64_t *next_key)
{
	void *value = NULL;
	int found = critnib_find(ids, key, FIND_GE, next_key, &value);
	assert(value == NULL);
	return found;
}

static bool id_manager_critnib_get_min_key(critnib *ids, uint64_t *key)
{
	return id_manager_critnib_get_ge_key(ids, 0, key);
}

static bool id_manager_critnib_get_max_key(critnib *ids, uint64_t *key)
{
	return id_manager_critnib_get_le_key(ids, UINT64_MAX, key);
}

uint64_t id_manager_acquire(struct id_manager *manager)
{
	uint64_t id;
	pthread_mutex_lock(&manager->id_acquire_release_mutex);

	uint64_t min_id;
	bool found = id_manager_critnib_get_min_key(manager->ids, &min_id);
	if (!found) {
		/* No id available in ids. */
		id = __atomic_fetch_add(&manager->next_id, 1, __ATOMIC_RELAXED);
	} else {
		critnib_remove(manager->ids, min_id);
		id = min_id;
	}

	pthread_mutex_unlock(&manager->id_acquire_release_mutex);

	return id;
}

static void id_manager_do_compaction(struct id_manager *manager)
{
	uint64_t id = manager->next_id;
	while (1) {
		/* Id is the biggest one from all granted ids. This means
		 * that we can try to do compaction (decrease size of the
		 * container and next_id value). */
		__atomic_fetch_sub(&manager->next_id, 1, __ATOMIC_RELAXED);

		uint64_t next_id;
		bool found = id_manager_critnib_get_le_key(manager->ids, id, &next_id);
		if (!found) {
			/* No more ids in the container. */
			break;
		}

		id = next_id;
		if (id == manager->next_id - 1) {
			/* We can continue compaction. */
			critnib_remove(manager->ids, id);
		} else {
			/* Cannot do compaction, some ids are still held by users. */
			break;
		}
	}
}

static bool id_manager_release_invariants(struct id_manager *manager)
{
	uint64_t max_key;
	bool found = id_manager_critnib_get_max_key(manager->ids, &max_key);
	if (found) {
		/* Otherwise compaction would have happened. */
		return max_key < manager->next_id - 1;
	}

	return true;
}

int id_manager_release(struct id_manager *manager, uint64_t id)
{
	int ret = 0;
	pthread_mutex_lock(&manager->id_acquire_release_mutex);

	/* At least one id must have been granted. */
	assert(manager->next_id >= 1);

	if (id != manager->next_id - 1) {
		ret = critnib_insert(manager->ids, id, NULL, 0);
	} else {
		id_manager_do_compaction(manager);
	}

	assert(id_manager_release_invariants(manager));
	pthread_mutex_unlock(&manager->id_acquire_release_mutex);
	return ret;
}

uint64_t id_manager_max_num_used(struct id_manager *manager)
{
	return __atomic_load_n(&manager->next_id, __ATOMIC_RELAXED);
}
