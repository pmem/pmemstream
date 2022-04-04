// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#ifndef LIBPMEMSTREAM_RANDOM_HELPERS_HPP
#define LIBPMEMSTREAM_RANDOM_HELPERS_HPP

#include <initializer_list>
#include <random>
#include <vector>

#include "unittest.hpp"

static std::mt19937_64 rnd_generator;

static inline void init_random()
{
	uint64_t seed;
	const char *seed_env = std::getenv("TEST_SEED");
	if (seed_env == NULL) {
		std::random_device rd;
		seed = rd();
		std::cout << "To reproduce set env variable TEST_SEED=" << seed << std::endl;
	} else {
		seed = std::stoull(seed_env);
		std::cout << "Running with TEST_SEED=" << seed << std::endl;
	}
	rnd_generator = std::mt19937_64(seed);
}

/* generates sequence of n commands out of 'commands' list */
template <typename T>
static inline std::vector<T> generate_commands(size_t n, std::initializer_list<T> commands)
{
	std::vector<T> out;
	for (size_t i = 0; i < n; i++) {
		/* puts back to 'out' a single element from 'commands' list */
		const size_t samples_number = 1;
		std::sample(commands.begin(), commands.end(), std::back_inserter(out), samples_number, rnd_generator);
	}
	return out;
}

#endif /* LIBPMEMSTREAM_RANDOM_HELPERS_HPP */
