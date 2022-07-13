# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

include(${TESTS_ROOT_DIR}/cmake/exec_functions.cmake)

setup()

execute(${EXECUTABLE} --path ${DIR}/testfile-pmemstream)
execute(${EXECUTABLE} --path ${DIR}/testfile-pmemstream --concurrency 3 --size 10485760 --region_size 2621440)
execute(${EXECUTABLE} --engine pmemlog --path ${DIR}/testfile-pmemlog)

finish()
