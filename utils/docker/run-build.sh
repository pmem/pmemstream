#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2022, Intel Corporation

#
# run-build.sh [build_step]...
#		in CI it's run inside a Docker container, called by ./build.sh .
#		It can be also run locally (but with caution, it may affect local environment).
#		Executes pmemstream builds (given as params; defined here as functions).
#

set -e

source $(dirname ${0})/prepare-for-build.sh

# params set for this file; if not previously set, the right-hand param is used
TEST_DIR=${PMEMSTREAM_TEST_DIR:-${DEFAULT_TEST_DIR}}
CHECK_CPP_STYLE=${CHECK_CPP_STYLE:-ON}
TESTS_LONG=${TESTS_LONG:-OFF}
TESTS_USE_FORCED_PMEM=${TESTS_USE_FORCED_PMEM:-ON}
TESTS_ASAN=${TESTS_ASAN:-OFF}
TESTS_UBSAN=${TESTS_UBSAN:-OFF}
TEST_TIMEOUT=${TEST_TIMEOUT:-600}
TESTS_PMREORDER=${TESTS_PMREORDER:-ON}

###############################################################################
# BUILD tests_clang_debug_cpp17_no_valgrind llvm
###############################################################################
function tests_clang_debug_cpp17_no_valgrind() {
	printf "\n$(tput setaf 1)$(tput setab 7)BUILD ${FUNCNAME[0]} START$(tput sgr 0)\n"
	mkdir build
	cd build

	PKG_CONFIG_PATH=${PKG_CONFIG_PATH}:/opt/pmdk/lib/pkgconfig/ \
	CC=clang CXX=clang++ \
	cmake .. -DDEVELOPER_MODE=1 \
		-DCHECK_CPP_STYLE=${CHECK_CPP_STYLE} \
		-DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} \
		-DTRACE_TESTS=1 \
		-DCOVERAGE=${COVERAGE} \
		-DCXX_STANDARD=17 \
		-DTESTS_PMREORDER=${TESTS_PMREORDER} \
		-DTESTS_USE_VALGRIND=0 \
		-DTESTS_LONG=${TESTS_LONG} \
		-DTEST_DIR=${TEST_DIR} \
		-DTESTS_USE_FORCED_PMEM=${TESTS_USE_FORCED_PMEM} \
		-DUSE_ASAN=${TESTS_ASAN} \
		-DUSE_UBSAN=${TESTS_UBSAN} \
		-DBUILD_BENCHMARKS=1

	make -j$(nproc)
	ctest --output-on-failure --timeout ${TEST_TIMEOUT}
	if [ "${COVERAGE}" == "1" ]; then
		upload_codecov tests_clang_debug_cpp17
	fi

	workspace_cleanup
	printf "$(tput setaf 1)$(tput setab 7)BUILD ${FUNCNAME[0]} END$(tput sgr 0)\n\n"
}

###############################################################################
# BUILD tests_clang_release_cpp17_no_valgrind llvm
###############################################################################
function tests_clang_release_cpp17_no_valgrind() {
	printf "\n$(tput setaf 1)$(tput setab 7)BUILD ${FUNCNAME[0]} START$(tput sgr 0)\n"
	mkdir build
	cd build

	PKG_CONFIG_PATH=${PKG_CONFIG_PATH}:/opt/pmdk/lib/pkgconfig/ \
	CC=clang CXX=clang++ \
	cmake .. -DDEVELOPER_MODE=1 \
		-DCHECK_CPP_STYLE=${CHECK_CPP_STYLE} \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} \
		-DTRACE_TESTS=1 \
		-DCOVERAGE=${COVERAGE} \
		-DCXX_STANDARD=17 \
		-DTESTS_USE_VALGRIND=0 \
		-DTESTS_LONG=${TESTS_LONG} \
		-DTEST_DIR=${TEST_DIR} \
		-DTESTS_PMREORDER=${TESTS_PMREORDER} \
		-DTESTS_USE_FORCED_PMEM=${TESTS_USE_FORCED_PMEM} \
		-DUSE_ASAN=${TESTS_ASAN} \
		-DUSE_UBSAN=${TESTS_UBSAN}

	make -j$(nproc)
	ctest --output-on-failure --timeout ${TEST_TIMEOUT}
	if [ "${COVERAGE}" == "1" ]; then
		upload_codecov tests_clang_debug_cpp17
	fi

	workspace_cleanup
	printf "$(tput setaf 1)$(tput setab 7)BUILD ${FUNCNAME[0]} END$(tput sgr 0)\n\n"
}

###############################################################################
# BUILD build_gcc_debug_cpp17 (no tests)
###############################################################################
function build_gcc_debug_cpp17() {
	mkdir build
	cd build

	PKG_CONFIG_PATH=${PKG_CONFIG_PATH}:/opt/pmdk/lib/pkgconfig/ \
	CC=gcc CXX=g++ \
	cmake .. -DDEVELOPER_MODE=1 \
		-DCHECK_CPP_STYLE=${CHECK_CPP_STYLE} \
		-DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} \
		-DTRACE_TESTS=1 \
		-DCOVERAGE=${COVERAGE} \
		-DCXX_STANDARD=17 \
		-DTESTS_USE_VALGRIND=1 \
		-DTEST_DIR=${TEST_DIR} \
		-DTESTS_PMREORDER=${TESTS_PMREORDER} \
		-DTESTS_USE_FORCED_PMEM=${TESTS_USE_FORCED_PMEM} \
		-DUSE_ASAN=${TESTS_ASAN} \
		-DUSE_UBSAN=${TESTS_UBSAN} \
		-DUSE_LIBUNWIND=1

	make -j$(nproc)
}



###############################################################################
# BUILD tests_gcc_debug_cpp17_no_valgrind
###############################################################################
function tests_gcc_debug_cpp17_no_valgrind() {
	printf "\n$(tput setaf 1)$(tput setab 7)BUILD ${FUNCNAME[0]} START$(tput sgr 0)\n"
	build_gcc_debug_cpp17
	ctest -E "_memcheck|_drd|_helgrind|_pmemcheck" --timeout ${TEST_TIMEOUT} --output-on-failure
	if [ "${COVERAGE}" == "1" ]; then
		upload_codecov tests_gcc_debug
	fi
	workspace_cleanup
	printf "$(tput setaf 1)$(tput setab 7)BUILD ${FUNCNAME[0]} END$(tput sgr 0)\n\n"
}

###############################################################################
# BUILD tests_gcc_debug_cpp17_valgrind
###############################################################################
function tests_gcc_debug_cpp17_valgrind() {
	printf "\n$(tput setaf 1)$(tput setab 7)BUILD ${FUNCNAME[0]} START$(tput sgr 0)\n"
	build_gcc_debug_cpp17
	ctest -R "_memcheck|_drd|_helgrind|_pmemcheck" --timeout ${TEST_TIMEOUT} --output-on-failure
	workspace_cleanup
	printf "$(tput setaf 1)$(tput setab 7)BUILD ${FUNCNAME[0]} END$(tput sgr 0)\n\n"
}

###############################################################################
# BUILD build_gcc_release_cpp17 (no tests)
###############################################################################
function build_gcc_release_cpp17() {
	mkdir build
	cd build

	PKG_CONFIG_PATH=${PKG_CONFIG_PATH}:/opt/pmdk/lib/pkgconfig/ \
	CC=gcc CXX=g++ \
	cmake .. -DDEVELOPER_MODE=1 \
		-DCHECK_CPP_STYLE=${CHECK_CPP_STYLE} \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} \
		-DTRACE_TESTS=1 \
		-DCOVERAGE=${COVERAGE} \
		-DCXX_STANDARD=17 \
		-DTESTS_USE_VALGRIND=1 \
		-DTEST_DIR=${TEST_DIR} \
		-DTESTS_PMREORDER=${TESTS_PMREORDER} \
		-DTESTS_USE_FORCED_PMEM=${TESTS_USE_FORCED_PMEM} \
		-DUSE_ASAN=${TESTS_ASAN} \
		-DUSE_UBSAN=${TESTS_UBSAN} \
		-DUSE_LIBUNWIND=1

	make -j$(nproc)
}

###############################################################################
# BUILD tests_gcc_debug_cpp17_no_valgrind
###############################################################################
function tests_gcc_release_cpp17_no_valgrind() {
	printf "\n$(tput setaf 1)$(tput setab 7)BUILD ${FUNCNAME[0]} START$(tput sgr 0)\n"
	build_gcc_release_cpp17
	ctest -E "_memcheck|_drd|_helgrind|_pmemcheck" --timeout ${TEST_TIMEOUT} --output-on-failure
	if [ "${COVERAGE}" == "1" ]; then
		upload_codecov tests_gcc_debug
	fi
	workspace_cleanup
	printf "$(tput setaf 1)$(tput setab 7)BUILD ${FUNCNAME[0]} END$(tput sgr 0)\n\n"
}

###############################################################################
# BUILD tests_package
###############################################################################
function tests_package() {
	printf "\n$(tput setaf 1)$(tput setab 7)BUILD ${FUNCNAME[0]} START$(tput sgr 0)\n"

	# building of packages should be verified only if PACKAGE_MANAGER equals 'rpm' or 'deb'
	case ${PACKAGE_MANAGER} in
		rpm|deb)
			# we're good to go
			;;
		*)
			echo "Notice: skipping building of packages because PACKAGE_MANAGER is not equal 'rpm' nor 'deb' ..."
			return 0
			;;
	esac

	# Fetch git history for `git describe` to work,
	# so that package has proper 'version' field
	[ -f .git/shallow ] && git fetch --unshallow --tags

	mkdir ${WORKDIR}/build
	pushd ${WORKDIR}/build

	CC=gcc CXX=g++ \
	cmake .. -DCMAKE_INSTALL_PREFIX=/usr \
		-DTESTS_USE_VALGRIND=0 \
		-DTESTS_LONG=OFF \
		-DBUILD_EXAMPLES=0 \
		-DCPACK_GENERATOR=${PACKAGE_MANAGER} \
		-DTESTS_USE_FORCED_PMEM=${TESTS_USE_FORCED_PMEM}

	echo "Make sure there is no library currently installed."
	echo "---------------------------- Error expected! ------------------------------"
	compile_example_standalone 01_basic_iterate && exit 1
	echo "---------------------------------------------------------------------------"

	make -j$(nproc) package

	if [ ${PACKAGE_MANAGER} = "deb" ]; then
		sudo_password dpkg -i libpmemstream*.deb
	elif [ ${PACKAGE_MANAGER} = "rpm" ]; then
		sudo_password rpm -i libpmemstream*.rpm
	fi

	echo "Verify installed package:"
	# prepare file with size > 0 (4MiB)
	dd if=/dev/urandom of=${WORKDIR}/build/testfile bs=1024 count=4096

	echo "Basic C example, run it twice for more entries:"
	compile_example_standalone 01_basic_iterate
	run_binary_standalone 01_basic_iterate ${WORKDIR}/build/testfile
	run_binary_standalone 01_basic_iterate ${WORKDIR}/build/testfile

	echo "C++ example, should print data from previous example:"
	compile_example_standalone 02_visual_iterator
	run_binary_standalone 02_visual_iterator ${WORKDIR}/build/testfile

	echo "C++ example using reserve-publish (with emplace new) instead of append:"
	compile_example_standalone 03_reserve_publish
	run_binary_standalone 03_reserve_publish ${WORKDIR}/build/testfile2

	echo "Basic append benchmark"
	compile_benchmark_standalone append
	run_binary_standalone benchmark-append --path ${WORKDIR}/build/testfile

	popd
	workspace_cleanup

	printf "$(tput setaf 1)$(tput setab 7)BUILD ${FUNCNAME[0]} END$(tput sgr 0)\n\n"
}

# Main
echo "### run-build.sh starts here ###"
workspace_cleanup

echo "Run build steps passed as script arguments"
build_steps=$@
echo "Defined build steps: ${build_steps}"

if [[ -z "${build_steps}" ]]; then
	echo "ERROR: The variable build_steps with selected builds to run is not set!"
	echo "Possible build steps:"
	grep "^function" ${0} | sed 's/function//' | cut -f1 -d"("
	exit 1
fi

for build in ${build_steps}
do
	${build}
done
