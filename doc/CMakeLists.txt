# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2022, Intel Corporation

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

# Generate files (to be installed) - links of C API functions (passed as {ARGN})
# to chosen (3) manpage.
function(add_manpage_links manpage)
	foreach(function ${ARGN})
		set(CONTENT ".so ${manpage}")
		file(WRITE ${MAN_DIR}/${function}.3 "${CONTENT}")
	endforeach()
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

	# libpmemstream.3
	configure_man(libpmemstream.3 ${CMAKE_CURRENT_SOURCE_DIR}/libpmemstream.3.md)
	# XXX: auto generate the list, based on libpmemstream.map file
	add_manpage_links(libpmemstream.3
		pmemstream_append pmemstream_async_append pmemstream_async_publish pmemstream_async_wait_committed
		pmemstream_async_wait_persisted pmemstream_committed_timestamp pmemstream_delete pmemstream_entry_data
		pmemstream_entry_iterator_delete pmemstream_entry_iterator_get pmemstream_entry_iterator_is_valid
		pmemstream_entry_iterator_new pmemstream_entry_iterator_next pmemstream_entry_iterator_seek_first
		pmemstream_entry_length pmemstream_entry_timestamp pmemstream_from_map pmemstream_persisted_timestamp
		pmemstream_publish pmemstream_region_allocate pmemstream_region_free pmemstream_region_iterator_delete
		pmemstream_region_iterator_get pmemstream_region_iterator_is_valid pmemstream_region_iterator_new
		pmemstream_region_iterator_next pmemstream_region_iterator_seek_first pmemstream_region_runtime_initialize
		pmemstream_region_size pmemstream_region_usable_size pmemstream_reserve)

	# prepare the actual 'make doc' command
	add_custom_target(doc ALL
		DEPENDS ${MANPAGE_OUTFILES}
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

	# install manpages
	install(FILES ${MAN_DIR}/libpmemstream.7
		DESTINATION ${CMAKE_INSTALL_MANDIR}/man7)
	install(DIRECTORY ${MAN_DIR}/
		DESTINATION ${CMAKE_INSTALL_MANDIR}/man3
		FILES_MATCHING
		PATTERN "*.3")
else()
	message(WARNING "pandoc not found - man pages will not be generated")
endif()
