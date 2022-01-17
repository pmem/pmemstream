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
script_dir=$(dirname ${0})
git rebase --exec "${script_dir}/run-build.sh  build_gcc_release_cpp17" ${base}
