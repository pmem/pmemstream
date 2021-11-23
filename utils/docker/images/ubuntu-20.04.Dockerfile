# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation

#
# Dockerfile - a 'recipe' for Docker to build an image of ubuntu-based
#              environment prepared for running pmemstream tests.
#

# Pull base image
FROM ghcr.io/pmem/libpmemobj-cpp:ubuntu-20.04-latest
MAINTAINER igor.chorazewicz@intel.com

USER root

RUN dpkg -i /opt/pmdk-pkg/*.deb

# Download scripts required in run-*.sh
COPY download-scripts.sh download-scripts.sh
COPY 0001-fix-generating-gcov-files-and-turn-off-verbose-log.patch 0001-fix-generating-gcov-files-and-turn-off-verbose-log.patch
RUN ./download-scripts.sh
