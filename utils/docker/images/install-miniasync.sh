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

# master: 0.2.1 + two fixes, 06.09.2022
MINIASYNC_VERSION="b79c5387e452056e1d29f6fa004b9ad7ecdf654b"
echo "MINIASYNC_VERSION: ${MINIASYNC_VERSION}"

build_dir=$(mktemp -d -t miniasync-XXX)

git clone https://github.com/pmem/miniasync ${build_dir}

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
