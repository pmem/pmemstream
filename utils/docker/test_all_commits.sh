#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

#
# test_all_commits.sh [base_commit]...
#		in CI it's run inside a Docker container, called by ./build.sh .
#		It can be also run locally (but with caution, it may affect local environment).
#

set -e

base=${1}

if [[ -z "${WORKDIR}" ]]; then
	echo "ERROR: The variable WORKDIR has to contain a path to the root " \
		"of this project"
	exit 1
fi

tmp_dir=$(mktemp -d -t pmemstream-XXXXX)
git clone ${WORKDIR} ${tmp_dir}

pushd ${tmp_dir}
git rebase --exec "WORKDIR=${tmp_dir} ${tmp_dir}/utils/docker/run-build.sh  build_gcc_release_cpp17" ${base}
popd
