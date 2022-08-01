#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

set -e

# params set for this file; if not previously set, the right-hand param is used
TEST_DIR=${PMEMSTREAM_TEST_DIR:-"/tmp/pmemstream_bench"}
CHECK_CPP_STYLE=${CHECK_CPP_STYLE:-OFF}
TESTS_LONG=${TESTS_LONG:-OFF}
TESTS_USE_FORCED_PMEM=${TESTS_USE_FORCED_PMEM:-ON}
TESTS_ASAN=${TESTS_ASAN:-OFF}
TESTS_UBSAN=${TESTS_UBSAN:-OFF}
TEST_TIMEOUT=${TEST_TIMEOUT:-600}
TESTS_PMREORDER=${TESTS_PMREORDER:-ON}
INSTALL_DIR="/usr/local"
STANDALONE_BUILD_DIR="/tmp/build_dir"

PMEMSTREAM_ADDR="https://github.com/pmem/pmemstream"
PMEMSTREAM_MASTER_ADDR=$PMEMSTREAM_ADDR
PMEMSTREAM_MASTER_COMMIT="master"
PMEMSTREAM_COMPARE_ADDR=$PMEMSTREAM_ADDR
PMEMSTREAM_COMPARE_PR=""
PMEMSTREAM_COMPARE_COMMIT=""

function usage()
{
	echo
	echo "Script for comparing pmemstream benchmarks."
	echo "Usage: $(basename $1) [-h|--help]  [-p|--pr] [-c|--commit] [--master_repo] [--master_commit] [--compare_repo]"
	echo "-h, --help       Print help and exit"
	echo "--pr             Pull Request to compare."
	echo "--commit         Commit or branch for comparing, if pr not specified."
	echo "--master_repo    Github repo address of forked pmemstream master [default: $PMEMSTREAM_MASTER_ADDR]"
	echo "--master_commit  Commit or branch for master [default: $PMEMSTREAM_MASTER_COMMIT]"
	echo "--compare_repo   Github repo address of forked pmemstream for comparing. Use 'commit' option instead 'pr' if repo fork is defferent than master. [default: $PMEMSTREAM_MASTER_ADDR]"
}

function compile_binary_standalone() {
	binary_name=${1}
	directory=${2}

	rm -rf ${STANDALONE_BUILD_DIR}
	mkdir ${STANDALONE_BUILD_DIR}
	pushd ${STANDALONE_BUILD_DIR}

	cmake ${TEST_DIR}/${directory}/${binary_name}

	# exit on error
	if [[ $? != 0 ]]; then
		popd
		return 1
	fi

	make -j$(nproc)
	popd
}

function run_binary_standalone() {
	binary_name=${1}
	optional_args=${@:2}
	echo "Run standalone application: ${binary_name} with arguments: ${optional_args}"

	pushd ${STANDALONE_BUILD_DIR}

	if [[ $TESTS_USE_FORCED_PMEM ]]; then
		PMEM2_FORCE_GRANULARITY=CACHE_LINE ./${binary_name} ${optional_args}
	else
		./${binary_name} ${optional_args}
	fi

	# exit on error
	if [[ $? != 0 ]]; then
		popd
		return 1
	fi

	popd
}

function build_pmemstream() {
	pmem_stream_folder=${1}

	cd $pmem_stream_folder
	mkdir build
	cd build

	PKG_CONFIG_PATH=${PKG_CONFIG_PATH}:/opt/pmdk/lib/pkgconfig/ \
	CC=clang CXX=clang++ \
	cmake .. -DDEVELOPER_MODE=1 \
		-DCHECK_CPP_STYLE=${CHECK_CPP_STYLE} \
		-DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} \
		-DTRACE_TESTS=1 \
		-DCXX_STANDARD=17 \
		-DTESTS_USE_VALGRIND=0 \
		-DBUILD_BENCHMARKS=1

	make -j$(nproc)
	sudo make -j$(nproc) install
}

function download_repo() {
	local source_folder_name=${1}
	local type=${2}
	local repo_id=${3}
	local repo_addr=${4}

	git clone $repo_addr $source_folder_name
	cd $source_folder_name
	if [[ "$type" == "PR" ]]; then
		git fetch origin pull/${repo_id}/head
		git checkout -b pullrequest FETCH_HEAD
	elif [[ "$type" == "commit" ]]; then
		git checkout ${repo_id}
	fi
}

function compare_output() {
	output_file_1=${1}
	output_file_2=${2}

	if [ ! -f $output_file_1 ] || [ ! -f $output_file_2 ]; then
		echo "Error: Output file not exist."
		return 1
	fi

	echo "*********************************************"
	echo "Comparing pmemstream master vs. compare"
	for (( i=6; i<=$(wc -l < $output_file_1); i++ )); do
		printf "%-40s %-15s %s\n" "$(sed -n ${i}p $output_file_1 | awk '{$1=$1}1')" "vs" "$(sed -n ${i}p $output_file_2 | awk '{$1=$1}1')"
	done
}

while getopts ":h-:" optchar; do
	case "${optchar}" in
		-)
		case "$OPTARG" in
			help) usage $0 && exit 0 ;;
			pr=*) PMEMSTREAM_COMPARE_PR="${OPTARG#*=}" ;;
			commit=*) PMEMSTREAM_COMPARE_COMMIT="${OPTARG#*=}" ;;
			master_repo=*) PMEMSTREAM_MASTER_ADDR="${OPTARG#*=}" ;;
			master_commit=*) PMEMSTREAM_MASTER_COMMIT="${OPTARG#*=}" ;;
			compare_repo=*) PMEMSTREAM_COMPARE_ADDR="${OPTARG#*=}" ;;
			*) echo "Invalid argument '$OPTARG'"; usage $0 && exit 1 ;;
		esac
		;;
		h) usage $0 && exit 0 ;;
		*) echo "Invalid argument '$OPTARG'"; usage $0 && exit 1 ;;
	esac
done

if [ ! -z $PMEMSTREAM_COMPARE_PR ]; then
	type="PR"
	repo_id=$PMEMSTREAM_COMPARE_PR
elif [ ! -z $PMEMSTREAM_MASTER_COMMIT ]; then
	type="commit"
	repo_id=$PMEMSTREAM_COMPARE_COMMIT
else
	echo "Error: Compare PR or Compare branch/commit not specified."
	exit 1
fi

rm -rf $TEST_DIR
mkdir $TEST_DIR

# prepare file with size > 0 (4MiB)
dd if=/dev/urandom of=${TEST_DIR}/testfile bs=1024 count=4096

cd $TEST_DIR && download_repo "pmemstream_master" commit $PMEMSTREAM_MASTER_COMMIT $PMEMSTREAM_MASTER_ADDR &
pid1=$!
cd $TEST_DIR && download_repo "pmemstream_compare" $type $repo_id $PMEMSTREAM_COMPARE_ADDR &
pid2=$!
wait $pid1 && wait $pid2

for pmemstream_case in "pmemstream_master" "pmemstream_compare"; do
	build_pmemstream $TEST_DIR/$pmemstream_case | tee -a $TEST_DIR/build_${pmemstream_case}.log
	cd $TEST_DIR/$pmemstream_case
	compile_binary_standalone append  $pmemstream_case/benchmarks
	run_binary_standalone benchmark-append --path ${TEST_DIR}/testfile | tee -a $TEST_DIR/bench_${pmemstream_case}.log
	cd $TEST_DIR/$pmemstream_case/build && sudo make uninstall
done

compare_output $TEST_DIR/bench_pmemstream_master.log $TEST_DIR/bench_pmemstream_compare.log
