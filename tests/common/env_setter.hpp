// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#ifndef LIBPMEMSTREAM_ENV_SETTER_HPP
#define LIBPMEMSTREAM_ENV_SETTER_HPP

#include "unittest.hpp"

#include <cstdlib>
#include <optional>
#include <string>

/* Sets environmental variable in its ctor - restores previous value (or unset)
 * the variable in its dtor. If replace is set to true, old value will be overwritten. */
struct env_setter {
	env_setter(const std::string &key, const std::string &value, bool replace) : key(key)
	{
		char *old_val = getenv(key.c_str());
		if (old_val && !replace) {
			std::cout << "Not replacing env var: " << key << " with value: " << old_val << std::endl;
			return;
		}

		if (old_val) {
			old_value = std::string(old_val);
			std::cout << "Replacing env var: " << key << " with value: " << old_val
				  << " with new value: " << value << std::endl;
		}

		int r = setenv(key.c_str(), value.c_str(), replace);
		UT_ASSERTeq(r, 0);
	}

	~env_setter()
	{
		if (!old_value) {
			int r = unsetenv(key.c_str());
			UT_ASSERTeq(r, 0);
		} else {
			setenv(key.c_str(), old_value->c_str(), 1);
		}
	}

	std::string key;
	std::optional<std::string> old_value = std::nullopt;
};

#endif /* LIBPMEMSTREAM_ENV_SETTER_HPP */
