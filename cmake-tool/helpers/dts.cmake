#
# Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
#
# SPDX-License-Identifier: BSD-2-Clause
#
cmake_minimum_required(VERSION 3.16.0)
include_guard(GLOBAL)

# This path is guaranteed to exist
find_path(DTS_PATH "sabre.dts" PATHS ${KERNEL_PATH}/tools/dts CMAKE_FIND_ROOT_PATH_BOTH)
# find a dts file matching platform.dts and put into a cache variable named
# <platform>_FOUND_DTS
function(FindDTS var platform)
    find_file(${platform}_FOUND_DTS ${platform}.dts PATHS ${DTS_PATH} CMAKE_FIND_ROOT_PATH_BOTH)
    if("${${platform}_FOUND_DTS}}" STREQUAL "${platform}_FOUND_DTS-NOTFOUND")
        message(WARNING "Could not find default dts file ${PLATFORM.dts}")
    endif()
    set(${var} ${${platform}_FOUND_DTS} PARENT_SCOPE)
endfunction()

# generate a dtb from a dts file using the dtc tool
function(GenDTB dts_file var)
    # find the dtc tool
    find_program(DTC_TOOL dtc)
    if("${DTC_TOOL}" STREQUAL "DTC_TOOL-NOTFOUND")
        message(FATAL_ERROR "Cannot find 'dtc' program.")
    endif()
    # check the dts we have been given exists
    if(NOT EXISTS "${dts_file}")
        message(FATAL_ERROR "Cannot find dts file ${dts_file}")
    endif()
    # put all the files into /dtb in the binary dir
    set(dtb_dir "${CMAKE_BINARY_DIR}/dtb")
    file(MAKE_DIRECTORY "${dtb_dir}")
    # generate a file name
    get_filename_component(FILE_NAME ${dts_file} NAME_WE)
    set(filename "${dtb_dir}/${FILE_NAME}.dtb")
    set(${var} ${filename} PARENT_SCOPE)
    # now add the command to generate the dtb
    execute_process(
        COMMAND
            ${DTC_TOOL} -I dts -O dtb -o ${filename} ${dts_file}
        OUTPUT_VARIABLE output
        ERROR_VARIABLE output
    )
    if(NOT EXISTS "${filename}")
        message(FATAL_ERROR "${output}
            failed to gen ${filename}")
    endif()
endfunction()
