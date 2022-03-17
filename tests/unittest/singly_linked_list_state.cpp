// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include "singly_linked_list.h"

#include <rapidcheck.h>

#include "rapidcheck_helpers.hpp"
#include "unittest.hpp"

#define RC_SLIST_UNORDERED (0U)
#define RC_SLIST_ASCENDING (1U << 0)
#define RC_SLIST_DESCENDING (1U << 1)

struct node {
	node(uint64_t d) : data(d), next(SLIST_INVALID_OFFSET)
	{
	}
	uint64_t data;
	uint64_t next;
};

void *memcpy_mock(void *dest, const void *src, size_t len, unsigned flags)
{
	return NULL;
}

void *memset_mock(void *dest, int c, size_t len, unsigned flags)
{
	return NULL;
}

void flush_mock(const void *ptr, size_t size)
{
	return;
}

void persist_mock(const void *ptr, size_t size)
{
	return;
}

void drain_mock(void)
{
	return;
}

/*
 * Simplified model of the list of nodes, where elements are represented as incremented
 * counter's value (which is unique). Each created node has this counter's value set as data.
 */
struct singly_linked_list_model {
	singly_linked_list_model(unsigned ord) : list(), counter(0), ordering(ord)
	{
	}
	std::list<uint64_t> list;
	size_t counter;
	unsigned ordering;
};

/*
 * SUT struct with NULL-ed begining of the spans.
 * It simplifies evaluating the pointer of each node (for the sake of testing).
 */
struct singly_linked_list_test {
	singly_linked_list_test()
	{
		SLIST_INIT(&sut);
		runtime = {.spans = NULL,
			   .memcpy = &memcpy_mock,
			   .memset = &memset_mock,
			   .flush = &flush_mock,
			   .drain = &drain_mock,
			   .persist = &persist_mock};
	}

	struct singly_linked_list sut;
	struct pmemstream_data_runtime runtime;
};

using singly_linked_list_command = rc::state::Command<singly_linked_list_model, singly_linked_list_test>;

struct singly_linked_list_insert_command : singly_linked_list_command {
	struct node *n = NULL;

	singly_linked_list_insert_command(const singly_linked_list_model &m)
	{
		n = new struct node(m.counter);
	}

	~singly_linked_list_insert_command()
	{
		delete n;
	}
};

struct rc_insert_head_command : singly_linked_list_insert_command {
	explicit rc_insert_head_command(const singly_linked_list_model &m) : singly_linked_list_insert_command(m)
	{
	}

	void apply(singly_linked_list_model &m) const override
	{
		m.list.push_front(n->data);
		m.counter++;
	}

	void run(const singly_linked_list_model &m, singly_linked_list_test &s) const override
	{
		SLIST_INSERT_HEAD(struct node, &s.runtime, &s.sut, (uint64_t)n, next);
	}
};

struct rc_insert_tail_command : singly_linked_list_insert_command {
	explicit rc_insert_tail_command(const singly_linked_list_model &m) : singly_linked_list_insert_command(m)
	{
	}

	void apply(singly_linked_list_model &m) const override
	{
		m.list.push_back(n->data);
		m.counter++;
	}

	void run(const singly_linked_list_model &m, singly_linked_list_test &s) const override
	{
		SLIST_INSERT_TAIL(struct node, &s.runtime, &s.sut, (uint64_t)n, next);
	}
};

struct rc_remove_head_command : singly_linked_list_command {
	void checkPreconditions(const singly_linked_list_model &m) const override
	{
		RC_PRE(m.list.size() > 0);
	}

	void apply(singly_linked_list_model &m) const override
	{
		m.list.pop_front();
	}

	void run(const singly_linked_list_model &m, singly_linked_list_test &s) const override
	{
		SLIST_REMOVE_HEAD(struct node, &s.runtime, &s.sut, next);
	}
};

struct rc_remove_command : singly_linked_list_command {
	uint64_t data_to_remove;

	void checkPreconditions(const singly_linked_list_model &m) const override
	{
		RC_PRE(m.list.size() > 0);
		/* make sure it's still there before we apply it */
		auto found = std::find(m.list.begin(), m.list.end(), data_to_remove);
		RC_PRE(found != m.list.end());
	}

	explicit rc_remove_command(const singly_linked_list_model &m)
	{
		data_to_remove = *rc::gen::elementOf(m.list);
	}

	void apply(singly_linked_list_model &m) const override
	{
		m.list.remove(data_to_remove);
	}

	void run(const singly_linked_list_model &m, singly_linked_list_test &s) const override
	{
		/* find offset of a node with .data == 'data_to_remove' */
		auto it = s.sut.head;
		SLIST_FOREACH(struct node, &s.runtime, &s.sut, it, next)
		{
			auto it_data = (SLIST_GET_PTR(struct node, &s.runtime, it))->data;
			if (it_data == data_to_remove) {
				break;
			}
		}
		RC_ASSERT(it != SLIST_INVALID_OFFSET);

		SLIST_REMOVE(struct node, &s.runtime, &s.sut, it, next);
	}
};

struct rc_foreach_command : singly_linked_list_command {
	void run(const singly_linked_list_model &m, singly_linked_list_test &s) const override
	{
		auto m_it = m.list.begin();
		auto sut_it = s.sut.head;
		auto sut_it_next = SLIST_INVALID_OFFSET;
		SLIST_FOREACH(struct node, &s.runtime, &s.sut, sut_it, next)
		{
			auto sut_it_data = (SLIST_GET_PTR(struct node, &s.runtime, sut_it))->data;
			sut_it_next = SLIST_NEXT(struct node, &s.runtime, sut_it, next);

			RC_ASSERT(sut_it_data == *m_it);
			if (sut_it_next != SLIST_INVALID_OFFSET) {
				/* we are not at the end of the list yet */

				auto next_data = (SLIST_GET_PTR(struct node, &s.runtime, sut_it_next))->data;
				if (m.ordering == RC_SLIST_ASCENDING) {
					RC_ASSERT(sut_it_data < next_data);
				} else if (m.ordering == RC_SLIST_DESCENDING) {
					RC_ASSERT(sut_it_data > next_data);
				}
			}

			m_it++;
		}
		RC_ASSERT(sut_it_next == SLIST_INVALID_OFFSET);
	}
};

std::ostream &operator<<(std::ostream &os, const node &n)
{
	os << "data: " << n.data << ", next: " << n.next;
	return os;
}

int main(int argc, char *argv[])
{
	return run_test([&] {
		// XXX: can we easily change this only e.g. for memcheck...?
		// std::string rapidcheck_config = "noshrink=1 max_success=200"; //  max_size=20
		// env_setter setter("RC_PARAMS", rapidcheck_config, false);

		return_check ret;
		ret += rc::check("Random inserts and removes should not break a list", []() {
			singly_linked_list_model model(RC_SLIST_UNORDERED);
			singly_linked_list_test sut;

			rc::state::check(
				model, sut,
				rc::state::gen::execOneOfWithArgs<rc_insert_tail_command, rc_insert_head_command,
								  rc_remove_head_command, rc_remove_command,
								  rc_foreach_command>());
		});

		ret += rc::check("Tail inserts and random removes should produce ascending list", []() {
			singly_linked_list_model model(RC_SLIST_ASCENDING);
			singly_linked_list_test sut;

			rc::state::check(
				model, sut,
				rc::state::gen::execOneOfWithArgs<rc_insert_tail_command, rc_remove_head_command,
								  rc_remove_command, rc_foreach_command>());
		});

		ret += rc::check("Head inserts and random removes should produce descending list", []() {
			singly_linked_list_model model(RC_SLIST_DESCENDING);
			singly_linked_list_test sut;

			rc::state::check(
				model, sut,
				rc::state::gen::execOneOfWithArgs<rc_insert_head_command, rc_remove_head_command,
								  rc_remove_command, rc_foreach_command>());
		});
	});
}
