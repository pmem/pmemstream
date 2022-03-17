// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include "singly_linked_list.h"

#include <rapidcheck.h>

#include "rapidcheck_helpers.hpp"
#include "stream_helpers.hpp"
#include "unittest.hpp"

/* in some cases we expect list to be ordered */
enum ordering { unordered, ascending, descending };

/* to avoid null'ed data base we use arbitrary value
 * (UBSAN checker doesn't like "applying non-zero offset to null pointer") */
#define TEST_SLIST_DATA_BASE (1024UL)

struct node {
	node(uint64_t d) : data(d), next(SLIST_INVALID_OFFSET)
	{
	}
	uint64_t data;
	uint64_t next;
};

/*
 * Simplified model of the list of nodes, where elements are represented as incremented
 * counter's value (which is unique). Each created node has this counter's value set as data.
 */
struct singly_linked_list_model {
	singly_linked_list_model(ordering o) : list(), counter(0), order(o)
	{
	}
	std::list<uint64_t> list;
	size_t counter;
	ordering order;
};

/*
 * SUT struct with NULL-ed begining of the spans.
 * It simplifies evaluating the pointer of each node (for the sake of testing).
 */
struct singly_linked_list_test {
	singly_linked_list_test()
	{
		SLIST_INIT(&sut);
		runtime = {.base = (void *)TEST_SLIST_DATA_BASE,
			   .memcpy = &memcpy_mock,
			   .memset = &memset_mock,
			   .flush = &flush_mock,
			   .drain = &drain_mock,
			   .persist = &persist_mock};
	}

	struct singly_linked_list sut;
	struct pmemstream_runtime runtime;
};

using singly_linked_list_command = rc::state::Command<singly_linked_list_model, singly_linked_list_test>;

struct rc_insert_head_command : singly_linked_list_command {
	std::shared_ptr<struct node> node;

	explicit rc_insert_head_command(const singly_linked_list_model &m)
	{
		node = std::shared_ptr<struct node>(new struct node(m.counter), [](struct node *n) { delete n; });
	}

	void apply(singly_linked_list_model &m) const override
	{
		m.list.push_front(node.get()->data);
		/* 'apply' function is executed after the 'run', so the model is updated
		 * after the successful insert in SUT's (and model's) list. */
		m.counter++;
	}

	void run(const singly_linked_list_model &m, singly_linked_list_test &s) const override
	{
		SLIST_INSERT_HEAD(struct node, &s.runtime, &s.sut, (uint64_t)node.get() - TEST_SLIST_DATA_BASE, next);
	}
};

struct rc_insert_tail_command : singly_linked_list_command {
	std::shared_ptr<struct node> node;

	explicit rc_insert_tail_command(const singly_linked_list_model &m)
	{
		node = std::shared_ptr<struct node>(new struct node(m.counter), [](struct node *n) { delete n; });
	}

	void apply(singly_linked_list_model &m) const override
	{
		m.list.push_back(node.get()->data);
		/* 'apply' function is executed after the 'run', so the model is updated
		 * after the successful insert in SUT's (and model's) list. */
		m.counter++;
	}

	void run(const singly_linked_list_model &m, singly_linked_list_test &s) const override
	{
		SLIST_INSERT_TAIL(struct node, &s.runtime, &s.sut, (uint64_t)node.get() - TEST_SLIST_DATA_BASE, next);
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

	explicit rc_remove_command(const singly_linked_list_model &m)
	{
		data_to_remove = *rc::gen::elementOf(m.list);
	}

	void checkPreconditions(const singly_linked_list_model &m) const override
	{
		/* this check is run not only after ctor but also in case of shrinking */

		RC_PRE(m.list.size() > 0);
		/* make sure it's still there before we run this command */
		auto found = std::find(m.list.begin(), m.list.end(), data_to_remove);
		RC_PRE(found != m.list.end());
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
		/* we should have found an element with the expected data */
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
				if (m.order == ordering::ascending) {
					RC_ASSERT(sut_it_data < next_data);
				} else if (m.order == ordering::descending) {
					RC_ASSERT(sut_it_data > next_data);
				}
			}

			m_it++;
		}
		/* we reached the end of the model's list and sut_it->next points "beyond" the list */
		RC_ASSERT(m_it == m.list.end());
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
		// XXX: we could lower the TCs number for valgrind tracers (using valgrind header's macros)
		// std::string rapidcheck_config = "noshrink=1 max_success=10 max_size=10";
		// env_setter setter("RC_PARAMS", rapidcheck_config, false);

		return_check ret;
		ret += rc::check("Random inserts and removes should not break a list", []() {
			singly_linked_list_model model(ordering::unordered);
			singly_linked_list_test sut;

			/* mix here all available commands */
			rc::state::check(
				model, sut,
				rc::state::gen::execOneOfWithArgs<rc_insert_tail_command, rc_insert_head_command,
								  rc_remove_head_command, rc_remove_command,
								  rc_foreach_command>());
		});

		ret += rc::check("Tail inserts and random removes should produce ascending list", []() {
			singly_linked_list_model model(ordering::ascending);
			singly_linked_list_test sut;

			/* only do tail inserts - each next node in the list will have data bigger than previous one */
			rc::state::check(
				model, sut,
				rc::state::gen::execOneOfWithArgs<rc_insert_tail_command, rc_remove_head_command,
								  rc_remove_command, rc_foreach_command>());
		});

		ret += rc::check("Head inserts and random removes should produce descending list", []() {
			singly_linked_list_model model(ordering::descending);
			singly_linked_list_test sut;

			/* only do head inserts - each next node in the list will have data lower than previous one */
			rc::state::check(
				model, sut,
				rc::state::gen::execOneOfWithArgs<rc_insert_head_command, rc_remove_head_command,
								  rc_remove_command, rc_foreach_command>());
		});
	});
}
