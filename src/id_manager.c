// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include "id_manager.h"
#include "critnib/critnib.h"

#include <assert.h>
#include <pthread.h>
#include <stdlib.h>

struct id_manager {
	/* A set (key == value) of all available ids between 0 and next_id. */
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

static uint64_t critnib_value_to_id(void *value)
{
	/* Critnib holds id + 1 so that NULL (aka 0) can still mean not-found. */
	return ((uint64_t)value) - 1;
}

static void *id_to_critnib_value(uint64_t id)
{
	/* Critnib holds id + 1 so that NULL (aka 0) can still mean not-found. */
	return (void *)(id + 1);
}

static int get_critnib_min_key(uintptr_t key, void *value, void *d)
{
	uint64_t *data = (uint64_t *)d;

	assert(key == critnib_value_to_id(value));
	*data = key;

	/* Stop iteration. */
	return 1;
}

uint64_t id_manager_acquire(struct id_manager *manager)
{
	uint64_t id;
	pthread_mutex_lock(&manager->id_acquire_release_mutex);

	uint64_t min_id = UINT64_MAX;
	critnib_iter(manager->ids, 0, UINT64_MAX, get_critnib_min_key, &min_id);

	if (min_id == UINT64_MAX) {
		/* No id available in ids. */
		id = manager->next_id++;
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
		manager->next_id--;

		void *value = critnib_find_le(manager->ids, id);
		if (value == NULL) {
			/* No more ids in the container. */
			break;
		}

		id = critnib_value_to_id(value);
		if (id == manager->next_id - 1) {
			/* We can continue compaction. */
			critnib_remove(manager->ids, id);
		} else {
			/* Cannot do compaction, some ids are still held by users. */
			break;
		}
	}
}

int id_manager_release(struct id_manager *manager, uint64_t id)
{
	int ret = 0;
	pthread_mutex_lock(&manager->id_acquire_release_mutex);

	/* At least one id must have been granted. */
	assert(manager->next_id >= 1);

	if (id != manager->next_id - 1) {
		assert(id < manager->next_id);
		ret = critnib_insert(manager->ids, id, id_to_critnib_value(id), 0);
	} else {
		id_manager_do_compaction(manager);
	}

	pthread_mutex_unlock(&manager->id_acquire_release_mutex);
	return ret;
}
