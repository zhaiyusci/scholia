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
if(NOT DEFINED POPPLER_SOURCE OR "${POPPLER_SOURCE}" STREQUAL "")
    set(POPPLER_SOURCE "${SOURCE_ROOT}/external/poppler")
endif()
get_filename_component(POPPLER_SOURCE "${POPPLER_SOURCE}" ABSOLUTE)

if(NOT EXISTS "${SDK_PREFIX}/lib/zlib.lib")
    message(FATAL_ERROR "zlib is not installed in ${SDK_PREFIX}. Run build-zlib-sdk.cmake first.")
endif()
if(NOT EXISTS "${SDK_PREFIX}/lib/freetype.lib")
    message(FATAL_ERROR "freetype is not installed in ${SDK_PREFIX}. Run build-freetype-sdk.cmake first.")
endif()
if(NOT EXISTS "${POPPLER_SOURCE}/CMakeLists.txt")
    message(FATAL_ERROR "Cannot find Poppler source: ${POPPLER_SOURCE}")
endif()

set(BUILD_ROOT "${WORKSPACE_ROOT}/build/thirdparty")
set(POPPLER_BUILD "${BUILD_ROOT}/poppler-custom")

message(STATUS "Scholia SDK custom Poppler build")
message(STATUS "  Source    : ${POPPLER_SOURCE}")
message(STATUS "  SdkPrefix : ${SDK_PREFIX}")
message(STATUS "  QtPrefix  : ${QT_PREFIX}")

if(DEFINED CLEAN AND CLEAN)
    scholia_remove_inside("${POPPLER_BUILD}" "${BUILD_ROOT}")
endif()
scholia_prepare_build_dir("${POPPLER_BUILD}" "${BUILD_ROOT}" "${POPPLER_SOURCE}")

set(_prefix_path "${QT_PREFIX}\\;${SDK_PREFIX}")
set(_configure
    "\"${CMAKE_PROGRAM}\""
    -S "\"${POPPLER_SOURCE}\""
    -B "\"${POPPLER_BUILD}\""
    -G Ninja
    "-DCMAKE_MAKE_PROGRAM=\"${NINJA_PROGRAM}\""
    "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
    "-DCMAKE_INSTALL_PREFIX=\"${SDK_PREFIX}\""
    "-DCMAKE_PREFIX_PATH=\"${_prefix_path}\""
    -DBUILD_SHARED_LIBS=ON
    -DBUILD_TESTING=OFF
    -DBUILD_QT6_TESTS=OFF
    -DBUILD_QT5_TESTS=OFF
    -DBUILD_CPP_TESTS=OFF
    -DBUILD_GTK_TESTS=OFF
    -DBUILD_MANUAL_TESTS=OFF
    -DENABLE_UTILS=OFF
    -DENABLE_CPP=OFF
    -DENABLE_GLIB=OFF
    -DENABLE_GOBJECT_INTROSPECTION=OFF
    -DENABLE_GTK_DOC=OFF
    -DENABLE_QT5=OFF
    -DENABLE_QT6=ON
    -DENABLE_BOOST=OFF
    -DENABLE_LIBOPENJPEG=UnmaintainedWillBeRemovedInJuly2026
    -DENABLE_DCTDECODER=UnmaintainedWillBeRemovedInJuly2026
    -DENABLE_LCMS=OFF
    -DENABLE_LIBCURL=OFF
    -DENABLE_LIBTIFF=OFF
    -DENABLE_NSS3=OFF
    -DENABLE_GPGME=OFF
    -DENABLE_ZLIB_UNCOMPRESS=ON
    -DRUN_GPERF_IF_PRESENT=OFF
    -DFONT_CONFIGURATION=win32
)
list(JOIN _configure " " _configure_command)
scholia_run_vs("${_configure_command}")
scholia_run_vs("\"${CMAKE_PROGRAM}\" --build \"${POPPLER_BUILD}\" --target install --parallel ${JOBS}")

message(STATUS "custom Poppler installed into SDK.")
