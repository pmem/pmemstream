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

* 01_basic_iterate/main.c -- contains basic example workflow of C application.

* 02_visual_iterator/main.cpp -- it iterates over all entries, on previously
	created file, with pmemstream data (e.g. created using `01_basic_iterate` example)
	and prints these entries in a readable format (hex data or strings).
