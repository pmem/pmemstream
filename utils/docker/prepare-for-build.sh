#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2022, Intel Corporation

#
# prepare-for-build.sh - prepares environment for the build
#		(when ./build.sh was used, it happens inside a Docker container)
#		and stores functions common to run-* scripts in this dir.
#

set -e

INSTALL_DIR=/tmp/pmemstream
STANDALONE_BUILD_DIR=/tmp/build_dir
TEST_DIR=${PMEMSTREAM_TEST_DIR:-${DEFAULT_TEST_DIR}}
IGNORE_PATHS="--gi benchmarks --gi doc --gi examples --gi tests --gi utils --gi src/critnib"

### Helper functions, used in run-*.sh scripts
function sudo_password() {
	echo ${USERPASS} | sudo -Sk $*
}

function workspace_cleanup() {
	echo "Cleanup build dirs"

	pushd ${WORKDIR}
	rm -rf ${WORKDIR}/build
	rm -rf ${STANDALONE_BUILD_DIR}
	rm -rf ${INSTALL_DIR}
}

function upload_codecov() {
	# validate codecov.yaml file
	cat ${WORKDIR}/codecov.yml | curl --data-binary @- https://codecov.io/validate

	printf "\n$(tput setaf 1)$(tput setab 7)COVERAGE ${FUNCNAME[0]} START$(tput sgr 0)\n"

	# check if code was compiled with clang
	clang_used=$(cmake -LA -N . | grep -e "CMAKE_C.*_COMPILER" | grep clang | wc -c)
	if [[ ${clang_used} -gt 0 ]]; then
		# XXX: llvm-cov not supported
		echo "Warning: Llvm-cov is not supported"
		return 0
	fi

	# run codecov using gcov
	# we rely on parsed report on codecov.io
	/opt/scripts/codecov --verbose --flags ${1} --nonZero --gcov --gcovIgnore ${PATHS_TO_IGNORE} --rootDir . --clean

	echo "Check for any leftover gcov files"
	leftover_files=$(find . -name "*.gcov")
	if [[ -n "${leftover_files}" ]]; then
		# display found files and exit with error (they all should be parsed)
		echo "${leftover_files}"
		return 1
	fi

	printf "$(tput setaf 1)$(tput setab 7)COVERAGE ${FUNCNAME[0]} END$(tput sgr 0)\n\n"
}

function compile_binary_standalone() {
	binary_name=${1}
	directory=${2}

	rm -rf ${STANDALONE_BUILD_DIR}
	mkdir ${STANDALONE_BUILD_DIR}
	pushd ${STANDALONE_BUILD_DIR}

	cmake ${WORKDIR}/${directory}/${binary_name}

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

function compile_example_standalone() {
	example_name=${1}
	echo "Compile standalone example: ${example_name}"
	compile_binary_standalone ${example_name} examples
}

function compile_benchmark_standalone() {
	benchmark_name=${1}
	echo "Compile standalone benchmark: ${benchmark_name}"
	compile_binary_standalone ${benchmark_name} benchmarks
}

### Additional checks, to be run, when this file is sourced
if [[ -z "${WORKDIR}" ]]; then
	echo "ERROR: The variable WORKDIR has to contain a path to the root " \
		"of this project - 'build' sub-directory will be created there."
	exit 1
fi

# this should be run only on CIs
if [ "${CI_RUN}" == "YES" ]; then
	echo "CI build: change WORKDIR's owner and prepare tmpfs device"
	sudo_password chown -R $(id -u).$(id -g) ${WORKDIR}

	sudo_password mkdir ${TEST_DIR}
	sudo_password chmod 0777 ${TEST_DIR}
	sudo_password mount -o size=2G -t tmpfs none ${TEST_DIR}
fi || true

echo "CMake version:"
cmake --version

# assign CMake's version to variable(s)  - a single number representation for easier comparison
CMAKE_VERSION=$(cmake --version | head -n1 | grep -P -o "\d+\.\d+")
CMAKE_VERSION_MAJOR=$(echo ${CMAKE_VERSION} | cut -d. -f1)
CMAKE_VERSION_MINOR=$(echo ${CMAKE_VERSION} | cut -d. -f2)
CMAKE_VERSION_NUMBER=${CMAKE_VERSION_MAJOR}${CMAKE_VERSION_MINOR}
