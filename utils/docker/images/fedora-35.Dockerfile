# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021-2022, Intel Corporation

#
# Dockerfile - a 'recipe' for Docker to build an image of ubuntu-based
#              environment prepared for running pmemstream tests.
#

# Pull base image
FROM ghcr.io/pmem/dev-utils-kit:fedora-35-latest
MAINTAINER igor.chorazewicz@intel.com

# use 'root' while building the image
USER root

# Install all PMDK packages
# Use non-released ("custom") version with the fix for proper ndctl header include
ENV PMDK_VERSION bbd93c8c4c3ca8bc4d1136ad30b3bc15fa78919a
RUN /opt/install-pmdk.sh /opt/pmdk/

# Install rapidcheck
COPY install-rapidcheck.sh install-rapidcheck.sh
RUN ./install-rapidcheck.sh

COPY install-miniasync.sh install-miniasync.sh
RUN ./install-miniasync.sh

# switch back to regular user
USER ${USER}
