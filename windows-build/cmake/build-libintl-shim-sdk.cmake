cmake_minimum_required(VERSION 3.20)

get_filename_component(_script_dir "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
get_filename_component(_windows_build_dir "${_script_dir}/.." ABSOLUTE)
get_filename_component(_default_source_root "${_windows_build_dir}/.." ABSOLUTE)
get_filename_component(_default_workspace_root "${_default_source_root}/../windows_build" ABSOLUTE)
include("${_script_dir}/kf6-sdk-common.cmake")

scholia_default_path(SOURCE_ROOT "${_default_source_root}")
scholia_default_path(WORKSPACE_ROOT "${_default_workspace_root}")
scholia_default_path(SDK_PREFIX "${WORKSPACE_ROOT}/sdk")
scholia_find_toolchain()

if(NOT DEFINED BUILD_TYPE OR "${BUILD_TYPE}" STREQUAL "")
    set(BUILD_TYPE "RelWithDebInfo")
endif()
if(NOT DEFINED JOBS OR "${JOBS}" STREQUAL "")
    set(JOBS 8)
endif()

set(LIBINTL_SHIM_SOURCE "${SOURCE_ROOT}/windows-build/sdk-sources/libintl-shim")
set(BUILD_ROOT "${WORKSPACE_ROOT}/build/thirdparty")
set(LIBINTL_SHIM_BUILD "${BUILD_ROOT}/libintl-shim")

message(STATUS "Scholia SDK libintl shim build")
message(STATUS "  SdkPrefix : ${SDK_PREFIX}")

if(DEFINED CLEAN AND CLEAN)
    scholia_remove_inside("${LIBINTL_SHIM_BUILD}" "${BUILD_ROOT}")
endif()
scholia_prepare_build_dir("${LIBINTL_SHIM_BUILD}" "${BUILD_ROOT}" "${LIBINTL_SHIM_SOURCE}")

set(_configure
    "\"${CMAKE_PROGRAM}\""
    -S "\"${LIBINTL_SHIM_SOURCE}\""
    -B "\"${LIBINTL_SHIM_BUILD}\""
    -G Ninja
    "-DCMAKE_MAKE_PROGRAM=\"${NINJA_PROGRAM}\""
    "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
    "-DCMAKE_INSTALL_PREFIX=\"${SDK_PREFIX}\""
)
list(JOIN _configure " " _configure_command)
scholia_run_vs("${_configure_command}")
scholia_run_vs("\"${CMAKE_PROGRAM}\" --build \"${LIBINTL_SHIM_BUILD}\" --target install --parallel ${JOBS}")

message(STATUS "libintl shim installed into SDK.")
