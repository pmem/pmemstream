#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

# check_symbols.sh - Check if all symbols defined in linker map file are exposed in compiled library

set -e
set -x

map_file=${1}
so_file=${2}

symbols=$(awk -F';' '/global/{f="1";next} /local/{f=0} {if(f==1)print $1}' ${map_file})

for symbol in ${symbols}; do
	echo "checking symbol: ${symbol}"
	objdump -t ${so_file}  | grep "g.*F.*${symbol}"
done
