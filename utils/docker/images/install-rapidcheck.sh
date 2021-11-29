#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation

#
# install-rapidcheck.sh
#		- installs rapidcheck test framework
#

set -e

if [ "${SKIP_RAPIDCHECK_BUILD}" ]; then
	echo "Variable 'SKIP_RAPIDCHECK_BUILD' is set; skipping building rapidcheck"
	exit
fi

PREFIX="/usr"

# 10.10.2021: Merge pull request #277 from ezzieyguywuf/catch
RAPIDCHECK_VERSION="57e9d30b15984ae2601749828243ebc426e0dca0"
echo "RAPIDCHECK_VERSION: ${RAPIDCHECK_VERSION}"

build_dir=$(mktemp -d -t rapidcheck-XXX)

git clone https://github.com/emil-e/rapidcheck --shallow-since=2020-06-01 ${build_dir}

pushd ${build_dir}
git checkout ${RAPIDCHECK_VERSION}

mkdir build
cd build

# turn off all redundant components
cmake .. -DCMAKE_INSTALL_PREFIX=${PREFIX}

make -j$(nproc) install

popd
rm -r ${build_dir}
