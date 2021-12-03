# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2021, Intel Corporation

include(${TESTS_ROOT_DIR}/cmake/exec_functions.cmake)

setup()

# XXX: make this generic - replace dd and paremetrize count/bs
execute_process(COMMAND dd if=/dev/zero of=${DIR}/testfile bs=1024 count=1048576)
execute(${EXECUTABLE} ${DIR}/testfile)

finish()
