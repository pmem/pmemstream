# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021-2022, Intel Corporation

#
# Dockerfile - a 'recipe' for Docker to build an image of ubuntu-based
#              environment prepared for running pmemstream tests.
#

# Pull base image
FROM ghcr.io/pmem/dev-utils-kit:ubuntu-22.04-latest
MAINTAINER igor.chorazewicz@intel.com

# use 'root' while building the image
USER root

# Codecov - coverage tool (optional)
ARG CODECOV_DEPS="\
	curl \
	llvm"

# Misc for our builds/CI (optional)
ARG MISC_DEPS="\
	clang-format-11"

# Install all required packages
# XXX: workaround for clang (v.14): default DWARF5 is not compatible with valgrind 3.19
RUN apt-get update \
 && apt-get remove -y clang \
 && apt-get autoremove -y \
 && apt-get install -y --no-install-recommends \
	${CODECOV_DEPS} \
	${MISC_DEPS} \
    clang-13 \
&& rm -rf /var/lib/apt/lists/*

# XXX: workaround for clang (v.14): default DWARF5 is not compatible with valgrind 3.19
RUN sudo ln -s /usr/bin/clang-13 /usr/bin/clang \
 && sudo ln -s /usr/bin/clang++-13 /usr/bin/clang++

# Install all PMDK packages
RUN /opt/install-pmdk.sh /opt/pmdk/

# Install rapidcheck
COPY install-rapidcheck.sh install-rapidcheck.sh
RUN ./install-rapidcheck.sh

COPY install-miniasync.sh install-miniasync.sh
RUN ./install-miniasync.sh

# Download scripts required in run-*.sh
COPY download-scripts.sh download-scripts.sh
COPY 0001-fix-generating-gcov-files-and-turn-off-verbose-log.patch 0001-fix-generating-gcov-files-and-turn-off-verbose-log.patch
RUN ./download-scripts.sh

# switch back to regular user
USER ${USER}
