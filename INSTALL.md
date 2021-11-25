# Install / build guide

## Contents

- [Requirements](#requirements)
- [Build and run tests](#build-and-run-tests)
- [Install](#install)
- [Build packages](#build-packages)
- [Out-of-source builds](#out-of-source-builds)

## Requirements

In order to build libpmemstream, you need to have installed:

* **Linux 64-bit** (OSX and Windows are not yet supported), with at least:
    * C compiler (e.g. `gcc`)
    * [CMake](http://www.cmake.org) >= 3.16
* **libpmem2**, which is part of [PMDK](https://github.com/pmem/pmdk) - Persistent Memory Development Kit 1.10
* Used only for **testing**:
	* C++ compiler (e.g. `g++`)
	* [**valgrind**](https://github.com/pmem/valgrind) - tool for profiling and memory leak detection. *pmem* forked version with *pmemcheck*
		tool is recommended, but upstream/original [valgrind](https://valgrind.org/) is also compatible (package valgrind-devel is required).
* Used only for **development**:
	* [**pandoc**](https://pandoc.org/) - markup converter to generate man pages
	* [**perl**](https://www.perl.org/) - for whitespace checker script
	* [**clang format**](https://clang.llvm.org/docs/ClangFormat.html) - to format and check coding style

Required packages (or their names) for some OSes may differ. Some examples or scripts in
this repository may require additional dependencies, but should not interrupt the build.

See our **[Dockerfiles](utils/docker/images)** (used e.g. on our CI system)
to get an idea what packages are required to build the entire pmemstream,
with all tests, checks, and examples.

## Build and run tests

```sh
git clone https://github.com/pmem/pmemstream
cd pmemstream
mkdir ./build
cd ./build
cmake .. -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug    # run CMake, prepare Debug version
make -j$(nproc)                 # build everything
make test                       # run all tests
```

To see the output of failed tests, instead of the last command (`make test`) you can run:

```sh
ctest --output-on-failure
```

## Install

To package `pmemstream` as a shared library and install on your system:

```sh
cmake .. [-DCMAKE_BUILD_TYPE=Release]	# prepare e.g. Release version
sudo make install			# install shared library to the default location: /usr/local
sudo make uninstall			# remove shared library and headers
```

To install this library into other locations, pass appropriate value to cmake
using `CMAKE_INSTALL_PREFIX` variable like this:

```sh
cmake .. -DCMAKE_INSTALL_PREFIX=/usr [-DCMAKE_BUILD_TYPE=Release]
sudo make install		# install to path specified by CMAKE_INSTALL_PREFIX
sudo make uninstall		# remove shared library and headers from path specified by CMAKE_INSTALL_PREFIX
```

## Build packages

```sh
...
cmake .. -DCPACK_GENERATOR="$GEN" -DCMAKE_INSTALL_PREFIX=/usr [-DCMAKE_BUILD_TYPE=Release]
make -j$(nproc) package
```

where $GEN is a type of package generator and can be: RPM or DEB.

CMAKE_INSTALL_PREFIX must be set to a destination where packages will be installed.

## Out-of-source builds

If the standard build does not suit your needs, create your own
out-of-source build and run tests like this:

```sh
cd ~
mkdir mybuild
cd mybuild
cmake ~/pmemstream  # this directory should contain the source code of pmemstream
make -j$(nproc)
make test           # or 'ctest --output-on-failure'
```
