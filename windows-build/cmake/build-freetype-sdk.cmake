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
if(NOT DEFINED FREETYPE_VERSION OR "${FREETYPE_VERSION}" STREQUAL "")
    set(FREETYPE_VERSION "2.13.3")
endif()
if(NOT DEFINED FREETYPE_GIT_REF OR "${FREETYPE_GIT_REF}" STREQUAL "")
    string(REPLACE "." "-" _freetype_ref_version "${FREETYPE_VERSION}")
    set(FREETYPE_GIT_REF "VER-${_freetype_ref_version}")
endif()
if(NOT DEFINED FREETYPE_GIT_URL OR "${FREETYPE_GIT_URL}" STREQUAL "")
    set(FREETYPE_GIT_URL "https://gitlab.freedesktop.org/freetype/freetype.git")
endif()

set(SOURCE_CACHE "${WORKSPACE_ROOT}/sources")
set(BUILD_ROOT "${WORKSPACE_ROOT}/build/thirdparty")
set(FREETYPE_SOURCE "${SOURCE_CACHE}/freetype")
set(FREETYPE_BUILD "${BUILD_ROOT}/freetype")

message(STATUS "Scholia SDK freetype build")
message(STATUS "  Git ref   : ${FREETYPE_GIT_REF}")
message(STATUS "  SdkPrefix : ${SDK_PREFIX}")
message(STATUS "  QtPrefix  : ${QT_PREFIX}")

scholia_ensure_git_checkout("${FREETYPE_GIT_URL}" "${FREETYPE_GIT_REF}" "${FREETYPE_SOURCE}")
if(DEFINED CLEAN AND CLEAN)
    scholia_remove_inside("${FREETYPE_BUILD}" "${BUILD_ROOT}")
endif()
scholia_prepare_build_dir("${FREETYPE_BUILD}" "${BUILD_ROOT}" "${FREETYPE_SOURCE}")

set(_prefix_path "${QT_PREFIX}\\;${SDK_PREFIX}")
set(_configure
    "\"${CMAKE_PROGRAM}\""
    -S "\"${FREETYPE_SOURCE}\""
    -B "\"${FREETYPE_BUILD}\""
    -G Ninja
    "-DCMAKE_MAKE_PROGRAM=\"${NINJA_PROGRAM}\""
    "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
    "-DCMAKE_INSTALL_PREFIX=\"${SDK_PREFIX}\""
    "-DCMAKE_PREFIX_PATH=\"${_prefix_path}\""
    -DBUILD_SHARED_LIBS=ON
    -DFT_DISABLE_BROTLI=ON
    -DFT_DISABLE_BZIP2=ON
    -DFT_DISABLE_HARFBUZZ=ON
    -DFT_DISABLE_PNG=ON
    -DFT_DISABLE_ZLIB=ON
)
list(JOIN _configure " " _configure_command)
scholia_run_vs("${_configure_command}")
scholia_run_vs("\"${CMAKE_PROGRAM}\" --build \"${FREETYPE_BUILD}\" --target install --parallel ${JOBS}")

message(STATUS "freetype installed into SDK.")
