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
if(NOT DEFINED ZLIB_VERSION OR "${ZLIB_VERSION}" STREQUAL "")
    set(ZLIB_VERSION "1.3.1")
endif()
if(NOT DEFINED ZLIB_GIT_REF OR "${ZLIB_GIT_REF}" STREQUAL "")
    set(ZLIB_GIT_REF "v${ZLIB_VERSION}")
endif()
if(NOT DEFINED ZLIB_GIT_URL OR "${ZLIB_GIT_URL}" STREQUAL "")
    set(ZLIB_GIT_URL "https://github.com/madler/zlib.git")
endif()

set(SOURCE_CACHE "${WORKSPACE_ROOT}/sources")
set(BUILD_ROOT "${WORKSPACE_ROOT}/build/thirdparty")
set(ZLIB_SOURCE "${SOURCE_CACHE}/zlib")
set(ZLIB_BUILD "${BUILD_ROOT}/zlib")

message(STATUS "Scholia SDK zlib build")
message(STATUS "  Git ref   : ${ZLIB_GIT_REF}")
message(STATUS "  SdkPrefix : ${SDK_PREFIX}")
message(STATUS "  QtPrefix  : ${QT_PREFIX}")

scholia_ensure_git_checkout("${ZLIB_GIT_URL}" "${ZLIB_GIT_REF}" "${ZLIB_SOURCE}")
if(DEFINED CLEAN AND CLEAN)
    scholia_remove_inside("${ZLIB_BUILD}" "${BUILD_ROOT}")
endif()
scholia_prepare_build_dir("${ZLIB_BUILD}" "${BUILD_ROOT}" "${ZLIB_SOURCE}")

set(_configure
    "\"${CMAKE_PROGRAM}\""
    -S "\"${ZLIB_SOURCE}\""
    -B "\"${ZLIB_BUILD}\""
    -G Ninja
    "-DCMAKE_MAKE_PROGRAM=\"${NINJA_PROGRAM}\""
    "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
    "-DCMAKE_INSTALL_PREFIX=\"${SDK_PREFIX}\""
    -DSKIP_INSTALL_FILES=ON
)
list(JOIN _configure " " _configure_command)
scholia_run_vs("${_configure_command}")
scholia_run_vs("\"${CMAKE_PROGRAM}\" --build \"${ZLIB_BUILD}\" --target install --parallel ${JOBS}")

message(STATUS "zlib installed into SDK.")
