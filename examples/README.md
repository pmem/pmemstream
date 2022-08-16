# Examples

This directory contains C and C++ examples for pmemstream.
For more information about library see the top-level README.

## Building and execution

### Build with sources
To compile examples with current pmemstream sources, follow build steps for your OS
(as described in the top-level README). Make sure the BUILD_EXAMPLES options is ON.
The build directory should now contain `examples` sub-directory with all binaries,
ready to run, e.g.:

```sh
cd <build_dir>/examples
./example-01_basic_iterate file
```

If an example requires additional parameter it will print its usage,
otherwise it will run and print the execution results (if any).

### Standalone build
To compile any example as a standalone application (using pmemstream installed in the OS)
you have to enter selected example's sub-directory and run e.g.:

```sh
cd <repo_dir>/examples/01-iterate
mkdir build
cd build
cmake ..
make
./01_basic_iterate file
```

Similarly to previous section, if an example requires additional parameter
it will print its usage, otherwise it will run and print the execution results (if any).

## Descriptions and additional dependencies:

* 01_basic_iterate/main.c -- contains basic example workflow of C application:
	creating/opening pmemstream instance, iterating over regions and entries,
	appending data and reading it back.

* 02_visual_iterator/main.cpp -- iterates over all entries, on previously
	created file, with pmemstream data (e.g. created using `01_basic_iterate` example)
	and prints these entries in a readable format (hex data or strings).

* 03_reserve_publish/main.cpp -- demonstrates how to use `pmemstream_reserve` and `pmemstream_publish`
	(with custom write), instead of "the usual" `pmemstream_append` approach.

* 04_basic_async/main.cpp -- shows example usage of sync and async appends.
	Each async append is executed in a different region.

* 05_timestamp_based_order/main.cpp -- shows how to achieve global ordering of elements concurrently
	appended to multiple regions in a stream. Application operates in the region per thread manner.

Beside examples, there are two `examples_helpers` headers (`.h` and `.hpp`) with a helper functions for
shared functionalities. They are hidden in these headers not to obfuscate the examples and to write them
in one place for all examples.
