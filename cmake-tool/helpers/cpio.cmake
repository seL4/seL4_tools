#
# Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
#
# SPDX-License-Identifier: BSD-2-Clause
#

# This module contains functions for creating a cpio archive containing a list
# of input files and turning it into an object file that can be linked into a binary.

include_guard(GLOBAL)

set(MAKE_CPIO_TOOL
    "${CMAKE_CURRENT_LIST_DIR}/make_cpio.py"
    CACHE INTERNAL "" FORCE
)

# Function for declaring rules to build a cpio archive that can be linked
# into another target
function(MakeCPIO output_name input_files)
    cmake_parse_arguments(PARSE_ARGV 2 MAKE_CPIO "" "CPIO_SYMBOL" "DEPENDS")
    if(NOT "${MAKE_CPIO_UNPARSED_ARGUMENTS}" STREQUAL "")
        message(FATAL_ERROR "Unknown arguments to MakeCPIO")
    endif()
    set(archive_symbol "_cpio_archive")
    if(NOT "${MAKE_CPIO_CPIO_SYMBOL}" STREQUAL "")
        set(archive_symbol ${MAKE_CPIO_CPIO_SYMBOL})
    endif()

    separate_arguments(cmake_c_flags_sep NATIVE_COMMAND "${CMAKE_C_FLAGS}")
    if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
        list(APPEND cmake_c_flags_sep "${CMAKE_C_COMPILE_OPTIONS_TARGET}${CMAKE_C_COMPILER_TARGET}")
    endif()

    add_custom_command(
        OUTPUT ${output_name}
        COMMAND rm -f archive.${output_name}.cpio
        COMMAND ${MAKE_CPIO_TOOL} ${CMAKE_CURRENT_BINARY_DIR}/archive.${output_name}.cpio
                ${input_files}
        COMMAND
            sh -c
            "echo 'X.section ._archive_cpio,\"aw\"X.globl ${archive_symbol}, ${archive_symbol}_endX${archive_symbol}:X.incbin \"archive.${output_name}.cpio\"X${archive_symbol}_end:X' | tr X '\\n'"
            > ${output_name}.S
        COMMAND ${CMAKE_C_COMPILER} ${cmake_c_flags_sep} -c -o ${output_name} ${output_name}.S
        DEPENDS ${input_files} ${MAKE_CPIO_DEPENDS}
        VERBATIM
        BYPRODUCTS archive.${output_name}.cpio ${output_name}.S
        COMMENT "Generate CPIO archive ${output_name}"
    )
endfunction(MakeCPIO)
