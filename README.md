# pmemstream

[![Basic Tests](https://github.com/pmem/pmemstream/actions/workflows/basic.yml/badge.svg)](https://github.com/pmem/pmemstream/actions/workflows/basic.yml)
[![pmemstream version](https://img.shields.io/github/tag/pmem/pmemstream.svg)](https://github.com/pmem/pmemstream/releases/latest)
[![Coverity Scan Build Status](https://scan.coverity.com/projects/24120/badge.svg)](https://scan.coverity.com/projects/pmem-pmemstream)
[![Coverage Status](https://codecov.io/github/pmem/pmemstream/coverage.svg?branch=master)](https://app.codecov.io/gh/pmem/pmemstream/branch/master)
[![Language grade: C/C++](https://img.shields.io/lgtm/grade/cpp/g/pmem/pmemstream.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/pmem/pmemstream/context:cpp)

`pmemstream` is a logging data structure optimized for persistent memory.

*This is experimental pre-release software and should not be used in production systems.
APIs and file formats may change at any time without preserving backwards compatibility.
All known issues and limitations are logged as GitHub issues.*

**Libpmemstream** implements a pmem-optimized log data structure and provides stream-like access to the data.
It presents a contiguous logical address space, divided into regions, with log entries of arbitrary sizes.
We intend for this library to be a foundation for various, more complex higher-level solutions.

This library is a successor to [libpmemlog](https://pmem.io/pmdk/libpmemlog/). These two libraries are very similar
in basic concept, but *libpmemlog* was developed in a straightforward manner and does not allow easy extensions.

For more information, including **C API** documentation see [pmem.io/pmemstream](https://pmem.io/pmemstream).

![example pmemstream](doc/pmemstream.png)

## Table of contents
1. [Build and install](#build-and-install)
2. [Contact us](#contact-us)

## Build and install
[Installation guide](INSTALL.md) provides detailed instructions how to build and install
`pmemstream` from sources, build rpm and deb packages, and more.

## Contact us
For more information about **pmemstream**, please:
- read our whitepaper [XXX attached to release 0.2.1](XXX_link_TBD),
<!-- update contact points -->
- contact Igor Chorążewicz (igor.chorazewicz@intel.com) or Piotr Balcer (piotr.balcer@intel.com),
- or post on our **#pmem** Slack channel using
[this invite link](https://join.slack.com/t/pmem-io/shared_invite/enQtNzU4MzQ2Mzk3MDQwLWQ1YThmODVmMGFkZWI0YTdhODg4ODVhODdhYjg3NmE4N2ViZGI5NTRmZTBiNDYyOGJjYTIyNmZjYzQxODcwNDg)
or [Google group](https://groups.google.com/g/pmem).
