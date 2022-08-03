#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

#
# run-compare-bench.sh - builds baseline and custom repo/commit/PR; compares these two using
#	our benchmark and prints both results.
#
# XXX: we could extend this with options to enable custom benchmark with custom args
#

set -e

# params set for this file; if not previously set, the right-hand param is used
BUILD_DIR="/tmp/pmemstream/bench_builds"
TEST_PATH=${PMEMSTREAM_TEST_DIR:-"/tmp/pmemstream/benchfile"}
USE_FORCED_PMEM=${TESTS_USE_FORCED_PMEM:-ON}

DEFAULT_REPO_ADDR="https://github.com/pmem/pmemstream"
COMPARE_PR=""
COMPARE_REF="HEAD"
COMPARE_REPO_ADDR=${DEFAULT_REPO_ADDR}
BASELINE_REF="master"
BASELINE_REPO_ADDR=${DEFAULT_REPO_ADDR}

function print_usage() {
	echo
	echo "Script for comparing pmemstream benchmarks."
	echo "Usage: $(basename ${1}) [-h|--help]  [-p|--pr] [-r|--ref] [--compare_repo] [--baseline_ref] [--baseline_repo]"
	echo "-h, --help       Print help and exit"
	echo "--pr             Pull Request for comparing (if set, the 'ref' is not taken into account)"
	echo "--ref            Commit or branch for comparing, if PR not specified [default: ${COMPARE_REF}]"
	echo "--compare_repo   GitHub repo address for comparing. It makes sense when using 'ref' option (and repo is different than upstream) [default: ${COMPARE_REPO_ADDR}]"
	echo "--baseline_ref   Commit or branch for baseline [default: ${BASELINE_REF}]"
	echo "--baseline_repo  GitHub repo address for baseline (and its ref - commit sha or branch) [default: ${BASELINE_REPO_ADDR}]"
}

function download_repo() {
	local repo_subdir=${1}
	local ref_type=${2}
	local ref_value=${3}
	local repo_addr=${4}

	local repo_full_path=${BUILD_DIR}/${repo_subdir}
	mkdir -p ${repo_full_path}

	echo ""
	echo "### Downloading repo (${repo_addr}) with ref_type: ${ref_type}, ref_value: ${ref_value}"
	git clone ${repo_addr} ${repo_full_path}
	pushd ${repo_full_path}

	if [[ "${ref_type}" == "PR" ]]; then
		git fetch origin pull/${ref_value}/head
		git checkout -b pullrequest FETCH_HEAD
	elif [[ "${ref_type}" == "ref" ]]; then
		git checkout ${ref_value}
	fi

	popd
}

function build_repo() {
	case=${1}
	repo_path=${BUILD_DIR}/${case}

	mkdir -p ${repo_path}/build
	pushd ${repo_path}/build

	echo ""
	echo "### Building repo for case: ${case}"

	PKG_CONFIG_PATH=${PKG_CONFIG_PATH}:/opt/pmdk/lib/pkgconfig/ \
	cmake .. \
		-DCMAKE_BUILD_TYPE=Release \
		-DBUILD_BENCHMARKS=1 \
		-DBUILD_EXAMPLES=0 \
		-DBUILD_TESTS=0 \
		-DBUILD_DOC=0

	make -j$(nproc)

	popd
}

# XXX: this function should be more configurable
function run_benchmark() {
	case=${1}
	bench_path=${BUILD_DIR}/${case}/build/benchmarks/benchmark-append
	bench_cmd="${bench_path} --path ${TEST_PATH}_${case}"

	echo ""
	echo "### Running benchmark 'append' with default args for case: ${case}"
	if [[ ${USE_FORCED_PMEM} ]]; then
		PMEM2_FORCE_GRANULARITY=CACHE_LINE ${bench_cmd}
	else
		${bench_cmd}
	fi
}

function compare_output() {
	baseline_log=${1}
	compare_log=${2}

	if [ ! -f ${baseline_log} ] || [ ! -f ${compare_log} ]; then
		echo "Error: Output file(s) do not exist."
		return 1
	fi

	echo ""
	echo "*********************************************"
	echo "### Comparing 'baseline' vs. 'compare'"
	for (( i=6; i<=$(wc -l < ${baseline_log}); i++ )); do
		printf "%-40s %-15s %s\n" "$(sed -n ${i}p ${baseline_log} | awk '{$1=$1}1')" "vs" "$(sed -n ${i}p ${compare_log} | awk '{$1=$1}1')"
	done
}

### main ###
echo "Start of '${0}'"

while getopts ":h-:" optchar; do
	case "${optchar}" in
		-)
		case "${OPTARG}" in
			help) print_usage ${0} && exit 0 ;;
			pr=*) COMPARE_PR="${OPTARG#*=}" ;;
			ref=*) COMPARE_REF="${OPTARG#*=}" ;;
			compare_repo=*) COMPARE_REPO_ADDR="${OPTARG#*=}" ;;
			baseline_ref=*) BASELINE_REF="${OPTARG#*=}" ;;
			baseline_repo=*) BASELINE_REPO_ADDR="${OPTARG#*=}" ;;
			*) echo "Invalid argument '${OPTARG}'"; print_usage ${0} && exit 1 ;;
		esac
		;;
		h) print_usage ${0} && exit 0 ;;
		*) echo "Invalid argument '${OPTARG}'"; print_usage ${0} && exit 1 ;;
	esac
done

if [ -n "${COMPARE_PR}" ]; then
	ref_type="PR"
	ref_value=${COMPARE_PR}
elif [ -n "${BASELINE_REF}" ]; then
	ref_type="ref"
	ref_value=${COMPARE_REF}
else
	echo "Error: Compare PR or Compare ref not specified."
	exit 1
fi

echo "### Cleanup working and testing dirs"
rm -rf "${BUILD_DIR}"
rm -rf "${TEST_PATH}"
mkdir -p "${BUILD_DIR}"
mkdir -p $(basename ${TEST_PATH})


pushd ${BUILD_DIR}
download_repo "baseline" "ref" ${BASELINE_REF} ${BASELINE_REPO_ADDR}
download_repo "compare" ${ref_type} ${ref_value} ${COMPARE_REPO_ADDR}

echo "### Build and run benchmarks"
for case in "baseline" "compare"; do
	# Prepare file with size > 0 (10MiB)"
	dd if=/dev/urandom of="${TEST_PATH}_${case}" bs=1024 count=10240

	build_repo ${case} | tee -a ${BUILD_DIR}/build_${case}.log
	run_benchmark ${case} | tee -a ${BUILD_DIR}/bench_${case}.log
done

compare_output ${BUILD_DIR}/bench_baseline.log ${BUILD_DIR}/bench_compare.log

popd
