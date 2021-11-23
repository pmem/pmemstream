# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2021, Intel Corporation

#
# doc/CMakeLists.txt - prepares 'make doc' command for documentation
#

# Converts md files into manpage format. Requires pandoc command.
# name is output manpage name, input is a path to the source md file
function(configure_man name input)
	add_custom_command(OUTPUT ${MAN_DIR}/${name}
		MAIN_DEPENDENCY ${input}
		COMMAND ${PMEMSTREAM_ROOT_DIR}/utils/md2man/md2man.sh
			${input}
			${PMEMSTREAM_ROOT_DIR}/utils/md2man/default.man
			${MAN_DIR}/${name}
			${VERSION})
	list(APPEND MANPAGE_OUTFILES ${MAN_DIR}/${name})
	set(MANPAGE_OUTFILES ${MANPAGE_OUTFILES} PARENT_SCOPE)
endfunction()

# ----------------------------------------------------------------- #
## Setup custom targets and useful variables
# ----------------------------------------------------------------- #
set(MANPAGE_OUTFILES "")
set(MAN_DIR ${CMAKE_CURRENT_BINARY_DIR}/man)
add_check_whitespace(man ${CMAKE_CURRENT_SOURCE_DIR}/*.*)

# ----------------------------------------------------------------- #
## Prepare documentation (using manpage format)
# ----------------------------------------------------------------- #
find_program(PANDOC NAMES pandoc)
if(PANDOC)
	message(STATUS "Found pandoc: ${PANDOC}")

	# libpmemstream.7
	configure_man(libpmemstream.7 ${CMAKE_CURRENT_SOURCE_DIR}/libpmemstream.7.md)

	# prepare the actual 'make doc' command
	add_custom_target(doc ALL
		DEPENDS ${MANPAGE_OUTFILES}
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

	# install manpages
	install(FILES ${MAN_DIR}/libpmemstream.7
		DESTINATION ${CMAKE_INSTALL_MANDIR}/man7)
else()
	message(WARNING "pandoc not found - man pages will not be generated")
endif()
