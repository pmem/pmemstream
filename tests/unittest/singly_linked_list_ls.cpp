// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

/* XXX: fix name of the test file(s) for slist */

#include "singly_linked_list.h"

#include <cstring>
#include <vector>

#include <rapidcheck.h>

#include "rapidcheck_helpers.hpp"
#include "stream_helpers.hpp"
#include "unittest.hpp"

struct node {
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

struct singly_linked_list_model {
	singly_linked_list_model(std::vector<struct node> data)
	{
		offsets = {};
		data = data;
	}
	std::list<uint64_t> offsets;
	std::vector<struct node> data;
};

struct singly_linked_list_test {
	singly_linked_list_test(const std::vector<struct node> &data)
	{
		SLIST_INIT(&sut);
		runtime = {.spans = (uint64_t *)data.data(),
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

struct rc_insert_tail_command : public singly_linked_list_command {
	uint64_t offset_to_append;

	explicit rc_insert_tail_command(const singly_linked_list_model &m)
	{
		offset_to_append = (*rc::gen::elementOf(m.data)).data;
	}

	void apply(singly_linked_list_model &m) const override
	{
		m.offsets.push_back(offset_to_append);
	}

	void run(const singly_linked_list_model &m, singly_linked_list_test &s) const override
	{
		SLIST_INSERT_TAIL(struct node, &s.runtime, &s.sut, offset_to_append, next);
	}
};

struct rc_remove_head_command : public singly_linked_list_command {
	void checkPreconditions(const singly_linked_list_model &m) const override
	{
		RC_PRE(m.offsets.size() > 0);
	}

	void apply(singly_linked_list_model &m) const override
	{
		m.offsets.pop_front();
	}

	void run(const singly_linked_list_model &m, singly_linked_list_test &s) const override
	{
		SLIST_REMOVE_HEAD(struct node, &s.runtime, &s.sut, next);
	}
};

std::ostream &operator<<(std::ostream &os, const node &n)
{
	os << n.data << " " << n.next;
	return os;
}

namespace rc
{
template <>
struct Arbitrary<node> {
	static Gen<node> arbitrary()
	{
		return gen::construct<node>(gen::arbitrary<uint64_t>(), gen::arbitrary<uint64_t>());
	}
};
} // namespace rc

int main(int argc, char *argv[])
{
	return run_test([&] {
		rc::check("Random inserts and removes should not break a list",
			  [](const std::vector<struct node> &data) {
				  singly_linked_list_model model(data);
				  singly_linked_list_test sut(data);

				  rc::state::check(model, sut,
						   rc::state::gen::execOneOfWithArgs<rc_insert_tail_command,
										     rc_remove_head_command>());
			  });
	});
}
