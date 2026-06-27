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
if(NOT DEFINED KF6_VERSION OR "${KF6_VERSION}" STREQUAL "")
    set(KF6_VERSION "6.27.0")
endif()
if(NOT DEFINED ECM_GIT_URL OR "${ECM_GIT_URL}" STREQUAL "")
    set(ECM_GIT_URL "https://invent.kde.org/frameworks/extra-cmake-modules.git")
endif()
if(NOT DEFINED ECM_GIT_REF OR "${ECM_GIT_REF}" STREQUAL "")
    set(ECM_GIT_REF "v${KF6_VERSION}")
endif()

set(SOURCE_CACHE "${WORKSPACE_ROOT}/sources")
set(BUILD_ROOT "${WORKSPACE_ROOT}/build/kf6-sdk")
if(NOT DEFINED ECM_SOURCE OR "${ECM_SOURCE}" STREQUAL "")
    set(ECM_SOURCE "${SOURCE_CACHE}/extra-cmake-modules")
endif()
get_filename_component(ECM_SOURCE "${ECM_SOURCE}" ABSOLUTE)
set(ECM_BUILD "${BUILD_ROOT}/extra-cmake-modules")

message(STATUS "Scholia KF6 SDK bootstrap")
message(STATUS "  QtPrefix    : ${QT_PREFIX}")
message(STATUS "  SdkPrefix   : ${SDK_PREFIX}")
message(STATUS "  Workspace   : ${WORKSPACE_ROOT}")
message(STATUS "  CMake       : ${CMAKE_PROGRAM}")
message(STATUS "  Ninja       : ${NINJA_PROGRAM}")
message(STATUS "  MSVC env    : ${VCVARS}")
message(STATUS "  KF6 version : ${KF6_VERSION}")
message(STATUS "  ECM source  : ${ECM_SOURCE}")
message(STATUS "  ECM ref     : ${ECM_GIT_REF}")

file(MAKE_DIRECTORY "${WORKSPACE_ROOT}" "${SDK_PREFIX}" "${SOURCE_CACHE}" "${BUILD_ROOT}")

if(NOT DEFINED SKIP_ECM OR NOT SKIP_ECM)
    scholia_ensure_git_checkout("${ECM_GIT_URL}" "${ECM_GIT_REF}" "${ECM_SOURCE}")
    if(DEFINED CLEAN AND CLEAN)
        scholia_remove_inside("${ECM_BUILD}" "${BUILD_ROOT}")
    endif()
    scholia_prepare_build_dir("${ECM_BUILD}" "${BUILD_ROOT}" "${ECM_SOURCE}")

    set(_prefix_path "${QT_PREFIX}\\;${SDK_PREFIX}")
    set(_configure
        "\"${CMAKE_PROGRAM}\""
        -S "\"${ECM_SOURCE}\""
        -B "\"${ECM_BUILD}\""
        -G Ninja
        "-DCMAKE_MAKE_PROGRAM=\"${NINJA_PROGRAM}\""
        "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
        "-DCMAKE_INSTALL_PREFIX=\"${SDK_PREFIX}\""
        "-DCMAKE_PREFIX_PATH=\"${_prefix_path}\""
        -DBUILD_TESTING=OFF
    )
    list(JOIN _configure " " _configure_command)
    scholia_run_vs("${_configure_command}")
    scholia_run_vs("\"${CMAKE_PROGRAM}\" --build \"${ECM_BUILD}\" --target install --parallel ${JOBS}")
endif()

file(WRITE "${SDK_PREFIX}/scholia-sdk-env.cmake"
"set(SCHOLIA_SDK_PREFIX \"${SDK_PREFIX}\")
set(QT_PREFIX \"${QT_PREFIX}\")
set(CMAKE_PREFIX_PATH \"${QT_PREFIX};${SDK_PREFIX}\")
")

file(WRITE "${SDK_PREFIX}/scholia-sdk-bootstrap.json"
"{
  \"Generated\": \"$ENV{DATE} $ENV{TIME}\",
  \"QtPrefix\": \"${QT_PREFIX}\",
  \"SdkPrefix\": \"${SDK_PREFIX}\",
  \"WorkspaceRoot\": \"${WORKSPACE_ROOT}\",
  \"CMake\": \"${CMAKE_PROGRAM}\",
  \"Ninja\": \"${NINJA_PROGRAM}\",
  \"VcVars\": \"${VCVARS}\",
  \"Kf6Version\": \"${KF6_VERSION}\",
  \"EcmSource\": \"${ECM_SOURCE}\",
  \"EcmGitRef\": \"${ECM_GIT_REF}\"
}
")

message(STATUS "SDK bootstrap complete.")
