cmake_minimum_required(VERSION 3.20)

get_filename_component(_script_dir "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
get_filename_component(_windows_build_dir "${_script_dir}/.." ABSOLUTE)
get_filename_component(_default_source_root "${_windows_build_dir}/.." ABSOLUTE)
get_filename_component(_default_workspace_root "${_default_source_root}/../windows_build" ABSOLUTE)
include("${_script_dir}/kf6-sdk-common.cmake")

if(NOT DEFINED MODULE OR "${MODULE}" STREQUAL "")
    message(FATAL_ERROR "Pass -DMODULE=<kf6-module-name>, for example -DMODULE=kcoreaddons.")
endif()

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
if(NOT DEFINED GIT_REF OR "${GIT_REF}" STREQUAL "")
    set(GIT_REF "v${KF6_VERSION}")
endif()
if(NOT DEFINED GIT_URL OR "${GIT_URL}" STREQUAL "")
    set(GIT_URL "https://invent.kde.org/frameworks/${MODULE}.git")
endif()

if(NOT EXISTS "${SDK_PREFIX}/share/ECM/cmake/ECMConfig.cmake")
    message(FATAL_ERROR "ECM is not installed in ${SDK_PREFIX}. Run bootstrap-kf6-sdk.cmake first.")
endif()
scholia_find_msgfmt(MSGFMT "${SDK_PREFIX}")

set(SOURCE_CACHE "${WORKSPACE_ROOT}/sources")
set(BUILD_ROOT "${WORKSPACE_ROOT}/build/kf6-sdk")
set(MODULE_SOURCE "${SOURCE_CACHE}/${MODULE}")
set(MODULE_BUILD "${BUILD_ROOT}/${MODULE}")

message(STATUS "Scholia KF6 module build")
message(STATUS "  Module      : ${MODULE}")
message(STATUS "  Git URL     : ${GIT_URL}")
message(STATUS "  Git ref     : ${GIT_REF}")
message(STATUS "  QtPrefix    : ${QT_PREFIX}")
message(STATUS "  SdkPrefix   : ${SDK_PREFIX}")
message(STATUS "  Workspace   : ${WORKSPACE_ROOT}")
message(STATUS "  CMake       : ${CMAKE_PROGRAM}")
message(STATUS "  Ninja       : ${NINJA_PROGRAM}")
message(STATUS "  msgfmt      : ${MSGFMT}")

if(NOT DEFINED SKIP_CHECKOUT OR NOT SKIP_CHECKOUT)
    scholia_ensure_git_checkout("${GIT_URL}" "${GIT_REF}" "${MODULE_SOURCE}")
endif()
if(DEFINED CLEAN AND CLEAN)
    scholia_remove_inside("${MODULE_BUILD}" "${BUILD_ROOT}")
endif()
scholia_prepare_build_dir("${MODULE_BUILD}" "${BUILD_ROOT}" "${MODULE_SOURCE}")

set(_module_was_built OFF)
if(NOT DEFINED SKIP_BUILD OR NOT SKIP_BUILD)
    set(_prefix_path "${QT_PREFIX}\\;${SDK_PREFIX}")
    set(_configure
        "\"${CMAKE_PROGRAM}\""
        -S "\"${MODULE_SOURCE}\""
        -B "\"${MODULE_BUILD}\""
        -G Ninja
        "-DCMAKE_MAKE_PROGRAM=\"${NINJA_PROGRAM}\""
        "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
        "-DCMAKE_INSTALL_PREFIX=\"${SDK_PREFIX}\""
        "-DCMAKE_PREFIX_PATH=\"${_prefix_path}\""
        "-DGETTEXT_MSGFMT_EXECUTABLE=\"${MSGFMT}\""
        -DBUILD_TESTING=OFF
        -DBUILD_QCH=OFF
    )
    if(DEFINED EXTRA_CMAKE_ARGS AND NOT "${EXTRA_CMAKE_ARGS}" STREQUAL "")
        string(REPLACE "," ";" _extra_args "${EXTRA_CMAKE_ARGS}")
        foreach(_arg IN LISTS _extra_args)
            list(APPEND _configure "${_arg}")
        endforeach()
    endif()
    list(JOIN _configure " " _configure_command)
    scholia_run_vs("${_configure_command}")
    scholia_run_vs("\"${CMAKE_PROGRAM}\" --build \"${MODULE_BUILD}\" --target install --parallel ${JOBS}")
    set(_module_was_built ON)
endif()

if(_module_was_built)
    set(_manifest "${SDK_PREFIX}/scholia-sdk-modules.cmake")
    file(APPEND "${_manifest}"
"set(SCHOLIA_KF6_MODULE_${MODULE}_GIT_URL \"${GIT_URL}\")
set(SCHOLIA_KF6_MODULE_${MODULE}_GIT_REF \"${GIT_REF}\")
set(SCHOLIA_KF6_MODULE_${MODULE}_BUILD_TYPE \"${BUILD_TYPE}\")
")
    message(STATUS "Module manifest: ${_manifest}")
endif()

message(STATUS "KF6 module ready: ${MODULE}")
