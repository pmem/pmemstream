#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

#
# install-miniasync.sh - installs pmem/libminiasync
#

set -e

if [ "${SKIP_MINIASYNC_BUILD}" ]; then
	echo "Variable 'SKIP_MINIASYNC_BUILD' is set; skipping building libminiasync"
	exit
fi

PREFIX="/usr"
PACKAGE_TYPE=${PACKAGE_MANAGER,,} # make it lowercase

# TBD release, 11.08.2022
MINIASYNC_VERSION="7fa75e02f8d99393d8136cda2185e1af13622adb"
echo "MINIASYNC_VERSION: ${MINIASYNC_VERSION}"

build_dir=$(mktemp -d -t miniasync-XXX)

git clone https://github.com/kfilipek/miniasync ${build_dir}

pushd ${build_dir}
git checkout ${MINIASYNC_VERSION}

mkdir build
pushd build

cmake .. -DCPACK_GENERATOR=${PACKAGE_TYPE} -DCMAKE_INSTALL_PREFIX=${PREFIX}

make -j$(nproc) package
if [ "${PACKAGE_TYPE}" = "deb" ]; then
	dpkg -i libminiasync-dev*.deb
elif [ "${PACKAGE_TYPE}" = "rpm" ]; then
	rpm -iv libminiasync-devel*.rpm
fi

popd
popd
rm -r ${build_dir}
