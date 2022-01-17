// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include "thread_id.h"
#include "id_manager.h"

#include <assert.h>
#include <pthread.h>
#include <stdlib.h>

#define THREAD_ID_INVALID UINT64_MAX

/* This structure is kept in thread-local storage and contains per-thread id. */
struct thread_data {
	uint64_t id;
	struct id_manager *id_manager;
};

/* Structure which assigns ids to threads (holds thread to id mapping). */
struct thread_id {
	/* Key related to per-thread 'thread_data' object. */
	pthread_key_t key;

	/* Instance of id_manager to acquire/release ids. */
	struct id_manager *id_manager;
};

static void id_destructor(void *data)
{
	struct thread_data *thread_data = (struct thread_data *)data;

	/* There is no way to handle failure here - just ignore it. Failure will make
	 * it impossible to reacquire this id but it will not affect correctness. */
	(void)id_manager_release(thread_data->id_manager, thread_data->id);

	free(thread_data);
}

struct thread_id *thread_id_new(void)
{
	struct thread_id *thread_id = malloc(sizeof(*thread_id));
	if (!thread_id) {
		return NULL;
	}

	thread_id->id_manager = id_manager_new();
	if (!thread_id->id_manager) {
		goto id_manager_new_err;
	}

	int ret = pthread_key_create(&thread_id->key, id_destructor);
	if (ret) {
		goto key_create_err;
	}

	return thread_id;

key_create_err:
	id_manager_destroy(thread_id->id_manager);
id_manager_new_err:
	free(thread_id);
	return NULL;
}

void thread_id_destroy(struct thread_id *thread_id)
{
	// XXX: handle errors from those functions
	pthread_key_delete(thread_id->key);
	id_manager_destroy(thread_id->id_manager);

	free(thread_id);
}

uint64_t thread_id_get(struct thread_id *thread_id)
{
	uint64_t id = THREAD_ID_INVALID;

	void *td = pthread_getspecific(thread_id->key);
	if (td == NULL) {
		struct thread_data *thread_data = malloc(sizeof(*thread_data));
		if (!thread_data) {
			return THREAD_ID_INVALID;
		}

		id = id_manager_acquire(thread_id->id_manager);
		assert(id != THREAD_ID_INVALID); // XXX: make it FATAL_ERROR
		thread_data->id = id;
		thread_data->id_manager = thread_id->id_manager;

		pthread_setspecific(thread_id->key, thread_data);
	} else {
		id = ((struct thread_data *)td)->id;
	}

	return id;
}
