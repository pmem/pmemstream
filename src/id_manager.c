// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include "id_manager.h"

#include <assert.h>
#include <stdlib.h>

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

static int get_critnib_min_key(uintptr_t key, void *v, void *d)
{
	uint64_t value = (uint64_t)v;
	uint64_t *data = (uint64_t *)d;

	assert(key == value - 1);
	*data = key;

	/* Stop iteration. */
	return 1;
}

uint64_t id_manager_acquire_id(struct id_manager *manager)
{
	uint64_t id;
	pthread_mutex_lock(&manager->id_acquire_release_mutex);

	uint64_t min_id = (uint64_t)-1;
	critnib_iter(manager->ids, 0, (uint64_t)-1, get_critnib_min_key, &min_id);

	if (min_id == (uint64_t)-1) {
		/* No id available in ids. */
		id = manager->next_id++;
	} else {
		critnib_remove(manager->ids, min_id);
		id = min_id;
	}

	pthread_mutex_unlock(&manager->id_acquire_release_mutex);

	return id;
}

int id_manager_release_id(struct id_manager *manager, uint64_t id)
{
	int ret = 0;
	pthread_mutex_lock(&manager->id_acquire_release_mutex);

	/* At least one id must have been granted. */
	assert(manager->next_id >= 1);

	if (id != manager->next_id - 1) {
		assert(id < manager->next_id);
		/* We insert id + 1 so that NULL (aka 0) can still mean not-found. */
		ret = critnib_insert(manager->ids, id, (void *)(id + 1), 0);
	} else {
		while (1) {
			/* Id is the biggest one from all granted ids. This means
			 * that we can try to do compaction (decrease size of the
			 * container and next_id value). */
			manager->next_id--;

			void *found_id_ptr = critnib_find_le(manager->ids, id);
			if (found_id_ptr == NULL) {
				/* No more ids in the container. */
				break;
			}

			id = ((uint64_t)found_id_ptr) - 1;
			if (id == manager->next_id - 1) {
				/* We can continue compaction. */
				critnib_remove(manager->ids, id);
			} else {
				/* Cannot do compaction, some ids are still held by users. */
				break;
			}
		}
	}

	pthread_mutex_unlock(&manager->id_acquire_release_mutex);
	return ret;
}
