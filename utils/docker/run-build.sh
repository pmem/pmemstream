#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2021, Intel Corporation

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
		-DTESTS_USE_VALGRIND=0 \
		-DTESTS_LONG=${TESTS_LONG} \
		-DTEST_DIR=${TEST_DIR} \
		-DTESTS_USE_FORCED_PMEM=${TESTS_USE_FORCED_PMEM} \
		-DUSE_ASAN=${TESTS_ASAN} \
		-DUSE_UBSAN=${TESTS_UBSAN}

	make -j$(nproc)
	ctest --output-on-failure --timeout ${TEST_TIMEOUT}
#	if [ "${COVERAGE}" == "1" ]; then
#		upload_codecov tests_clang_debug_cpp17
#	fi

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
		-DTESTS_USE_FORCED_PMEM=${TESTS_USE_FORCED_PMEM} \
		-DUSE_ASAN=${TESTS_ASAN} \
		-DUSE_UBSAN=${TESTS_UBSAN}

	make -j$(nproc)
	ctest --output-on-failure --timeout ${TEST_TIMEOUT}
#	if [ "${COVERAGE}" == "1" ]; then
#		upload_codecov tests_clang_debug_cpp17
#	fi

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
#	if [ "${COVERAGE}" == "1" ]; then
#		upload_codecov tests_gcc_debug
#	fi
	workspace_cleanup
	printf "$(tput setaf 1)$(tput setab 7)BUILD ${FUNCNAME[0]} END$(tput sgr 0)\n\n"
}

###############################################################################
# BUILD tests_gcc_debug_cpp17_valgrind
###############################################################################
function tests_gcc_debug_cpp17_valgrind() {
	printf "\n$(tput setaf 1)$(tput setab 7)BUILD ${FUNCNAME[0]} START$(tput sgr 0)\n"
	build_gcc_debug_cpp17
	ctest -R " _memcheck|_drd|_helgrind|_pmemcheck" --timeout ${TEST_TIMEOUT} --output-on-failure
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
#	if [ "${COVERAGE}" == "1" ]; then
#		upload_codecov tests_gcc_debug
#	fi
	workspace_cleanup
	printf "$(tput setaf 1)$(tput setab 7)BUILD ${FUNCNAME[0]} END$(tput sgr 0)\n\n"
}

###############################################################################
# BUILD tests_package
###############################################################################
function tests_package() {
	printf "\n$(tput setaf 1)$(tput setab 7)BUILD ${FUNCNAME[0]} START$(tput sgr 0)\n"

	mkdir build
	cd build

	CC=gcc CXX=g++ \
	cmake .. -DCMAKE_INSTALL_PREFIX=/usr \
		-DTESTS_USE_VALGRIND=0 \
		-DTESTS_LONG=OFF \
		-DTESTS_PMREORDER=OFF \
		-DBUILD_EXAMPLES=0 \
		-DCPACK_GENERATOR=${PACKAGE_MANAGER} \
		-DTESTS_USE_FORCED_PMEM=${TESTS_USE_FORCED_PMEM}

	make -j$(nproc)
	ctest --output-on-failure --timeout ${TEST_TIMEOUT}

	make -j$(nproc) package

	## XXX: Add cmake file for standalone example compilation
	#echo "Make sure there is no library currently installed."
	#echo "---------------------------- Error expected! ------------------------------"
	#compile_example_standalone 01-iterate && exit 1
	#echo "---------------------------------------------------------------------------"

	if [ ${PACKAGE_MANAGER} = "deb" ]; then
		sudo_password dpkg -i libpmemstream*.deb
	elif [ ${PACKAGE_MANAGER} = "rpm" ]; then
		sudo_password rpm -i libpmemstream*.rpm
	fi

	workspace_cleanup

	## XXX: Add cmake file for standalone example compilation
	#echo "Verify installed package."
	#compile_example_standalone 01-iterate

	# Remove pkg-config and force cmake to use find_package while compiling example
	if [ ${PACKAGE_MANAGER} = "deb" ]; then
		sudo_password dpkg -r --force-all pkg-config
	elif [ ${PACKAGE_MANAGER} = "rpm" ]; then
		# most rpm based OSes use the 'pkgconf' name, only openSUSE uses 'pkg-config'
		sudo_password rpm -e --nodeps pkgconf || sudo_password rpm -e --nodeps pkg-config
	fi

	## XXX: Add cmake file for standalone example compilation
	#echo "Verify installed package using find_package."
	#compile_example_standalone 01-iterate

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
