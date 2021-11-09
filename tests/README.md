# Content

This directory contains tests for pmemstream and their helpers.
Directories contains:

- **cmake** - cmake and ctest helpers, including suppressions' files and CMake executions scripts,
- **common** - shared functions and libraries for all tests.

# Tests execution

Before executing tests it's required to build pmemstream's sources and tests.
See [INSTALLING.md](../INSTALLING.md) for details. There are CMake's options related
to tests - see all options in top-level CMakeLists.txt with prefix `TESTS_`.
One of them - `TESTS_USE_FORCED_PMEM=ON` speeds up tests execution on emulated pmem.
It's useful for long (e.g. concurrent) tests.

To run all tests execute:

```sh
make test
```

or, if you want to see an output/logs of failed tests, run:

```sh
ctest --output-on-failure
```

There are other parameters to use in `ctest` command. To see the full list read
[ctest(1) manpage](https://cmake.org/cmake/help/latest/manual/ctest.1.html).
