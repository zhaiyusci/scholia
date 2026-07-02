cmake_minimum_required(VERSION 3.20)

get_filename_component(_script_dir "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
get_filename_component(_windows_build_dir "${_script_dir}/.." ABSOLUTE)
get_filename_component(_default_source_root "${_windows_build_dir}/.." ABSOLUTE)
get_filename_component(_default_workspace_root "${_default_source_root}/../windows_build" ABSOLUTE)

function(scholia_default_path variable default_value)
    if(NOT DEFINED ${variable} OR "${${variable}}" STREQUAL "")
        set(${variable} "${default_value}" PARENT_SCOPE)
    else()
        get_filename_component(_absolute_value "${${variable}}" ABSOLUTE)
        set(${variable} "${_absolute_value}" PARENT_SCOPE)
    endif()
endfunction()

function(scholia_remove_inside path allowed_root)
    get_filename_component(_path "${path}" ABSOLUTE)
    get_filename_component(_allowed "${allowed_root}" ABSOLUTE)
    string(FIND "${_path}" "${_allowed}" _position)
    if(NOT _position EQUAL 0)
        message(FATAL_ERROR "Refusing to remove path outside ${_allowed}: ${_path}")
    endif()
    if(EXISTS "${_path}")
        file(REMOVE_RECURSE "${_path}")
    endif()
endfunction()

function(scholia_sync_tree source destination allowed_root)
    get_filename_component(_source "${source}" ABSOLUTE)
    get_filename_component(_destination "${destination}" ABSOLUTE)
    if(NOT IS_DIRECTORY "${_source}")
        message(FATAL_ERROR "Cannot find source directory: ${_source}")
    endif()

    scholia_remove_inside("${_destination}" "${allowed_root}")
    file(MAKE_DIRECTORY "${_destination}")
    file(COPY "${_source}/" DESTINATION "${_destination}")
endfunction()

function(scholia_validate_file root relative_path)
    if(NOT EXISTS "${root}/${relative_path}")
        message(FATAL_ERROR "Missing staged file: ${relative_path}")
    endif()
endfunction()

function(scholia_find_inno_setup out_var)
    set(_candidates)
    if(DEFINED ISCC AND NOT "${ISCC}" STREQUAL "")
        list(APPEND _candidates "${ISCC}")
    endif()
    if(DEFINED ENV{LOCALAPPDATA})
        list(APPEND _candidates "$ENV{LOCALAPPDATA}/Programs/Inno Setup 6/ISCC.exe")
    endif()
    if(DEFINED ENV{ProgramFiles})
        list(APPEND _candidates "$ENV{ProgramFiles}/Inno Setup 6/ISCC.exe")
    endif()
    list(APPEND _candidates
        "C:/Program Files/Inno Setup 6/ISCC.exe"
        "C:/Program Files (x86)/Inno Setup 6/ISCC.exe"
    )

    foreach(_candidate IN LISTS _candidates)
        if(EXISTS "${_candidate}")
            get_filename_component(_iscc "${_candidate}" ABSOLUTE)
            set(${out_var} "${_iscc}" PARENT_SCOPE)
            return()
        endif()
    endforeach()

    find_program(_iscc_from_path NAMES ISCC.exe ISCC)
    if(_iscc_from_path)
        set(${out_var} "${_iscc_from_path}" PARENT_SCOPE)
        return()
    endif()

    message(FATAL_ERROR "Cannot find Inno Setup compiler. Pass -DISCC=<path-to-ISCC.exe>.")
endfunction()

scholia_default_path(SOURCE_ROOT "${_default_source_root}")
scholia_default_path(WORKSPACE_ROOT "${_default_workspace_root}")
scholia_default_path(INSTALL_PREFIX "${WORKSPACE_ROOT}/install/scholia")
scholia_default_path(STAGE_ROOT "${WORKSPACE_ROOT}/dist/scholia-pdf/app")
scholia_default_path(STEMTEX_SUPPORT_STAGE_ROOT "${WORKSPACE_ROOT}/dist/scholia-stemtex-support/app")
scholia_default_path(OUTPUT_DIR "${WORKSPACE_ROOT}/dist")

if(NOT DEFINED VERSION OR "${VERSION}" STREQUAL "")
    file(READ "${SOURCE_ROOT}/VERSION.txt" VERSION)
    string(STRIP "${VERSION}" VERSION)
endif()
if(NOT VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "VERSION must be MAJOR.MINOR.PATCH, got '${VERSION}'")
endif()

if(NOT DEFINED FILE_VERSION OR "${FILE_VERSION}" STREQUAL "")
    set(FILE_VERSION "${VERSION}.0")
endif()

if(DEFINED CLEAN_STAGE AND CLEAN_STAGE)
    scholia_remove_inside("${STAGE_ROOT}" "${WORKSPACE_ROOT}")
endif()

message(STATUS "Scholia Windows package stage")
message(STATUS "  Source root : ${SOURCE_ROOT}")
message(STATUS "  Install     : ${INSTALL_PREFIX}")
message(STATUS "  Stage       : ${STAGE_ROOT}")
message(STATUS "  Support     : ${STEMTEX_SUPPORT_STAGE_ROOT}")
message(STATUS "  Output      : ${OUTPUT_DIR}")
message(STATUS "  Version     : ${VERSION}")

scholia_validate_file("${INSTALL_PREFIX}" "bin/scholia.exe")
scholia_sync_tree("${INSTALL_PREFIX}/bin" "${STAGE_ROOT}/bin" "${STAGE_ROOT}")
file(REMOVE "${STAGE_ROOT}/bin/vc_redist.x64.exe")

if(IS_DIRECTORY "${INSTALL_PREFIX}/share/poppler")
    scholia_sync_tree("${INSTALL_PREFIX}/share/poppler" "${STAGE_ROOT}/share/poppler" "${STAGE_ROOT}")
else()
    scholia_remove_inside("${STAGE_ROOT}/share/poppler" "${STAGE_ROOT}")
    message(WARNING "Poppler CMap/CID data was not found under ${INSTALL_PREFIX}/share/poppler.")
endif()

if(IS_DIRECTORY "${INSTALL_PREFIX}/StemTeX")
    scholia_validate_file("${INSTALL_PREFIX}" "StemTeX/runtime/bin/sdk/stemtex-renderer.dll")
    scholia_validate_file("${INSTALL_PREFIX}" "StemTeX/runtime/bin/windows/xetexdaemon.exe")
    scholia_validate_file("${INSTALL_PREFIX}" "StemTeX/gui/profiles")
    scholia_sync_tree("${INSTALL_PREFIX}/StemTeX" "${STAGE_ROOT}/StemTeX" "${STAGE_ROOT}")
    scholia_remove_inside("${STAGE_ROOT}/StemTeX/runtime/texmf-dist" "${STAGE_ROOT}")
    scholia_remove_inside("${STAGE_ROOT}/StemTeX/runtime/texmf-var/fonts" "${STAGE_ROOT}")
    scholia_remove_inside("${STAGE_ROOT}/StemTeX/runtime/texmf-var/cache-warmup-renders" "${STAGE_ROOT}")
    scholia_remove_inside("${STAGE_ROOT}/StemTeX/runtime/texmf-var/cache-warmup-state" "${STAGE_ROOT}")
    file(REMOVE "${STAGE_ROOT}/StemTeX/runtime/texmf-var/xdvipdfmx-init-trace.log")
else()
    scholia_remove_inside("${STAGE_ROOT}/StemTeX" "${STAGE_ROOT}")
    message(WARNING "StemTeX runtime was not found under ${INSTALL_PREFIX}/StemTeX.")
endif()

set(_has_stemtex_support OFF)
if(IS_DIRECTORY "${INSTALL_PREFIX}/StemTeX/runtime/texmf-dist")
    set(_has_stemtex_support ON)
    scholia_remove_inside("${STEMTEX_SUPPORT_STAGE_ROOT}" "${WORKSPACE_ROOT}")
    scholia_sync_tree("${INSTALL_PREFIX}/StemTeX/runtime/texmf-dist" "${STEMTEX_SUPPORT_STAGE_ROOT}/StemTeX/runtime/texmf-dist" "${STEMTEX_SUPPORT_STAGE_ROOT}")
    if(IS_DIRECTORY "${INSTALL_PREFIX}/StemTeX/runtime/texmf-var")
        scholia_sync_tree("${INSTALL_PREFIX}/StemTeX/runtime/texmf-var" "${STEMTEX_SUPPORT_STAGE_ROOT}/StemTeX/runtime/texmf-var" "${STEMTEX_SUPPORT_STAGE_ROOT}")
        scholia_remove_inside("${STEMTEX_SUPPORT_STAGE_ROOT}/StemTeX/runtime/texmf-var/fonts/conf" "${STEMTEX_SUPPORT_STAGE_ROOT}")
        scholia_remove_inside("${STEMTEX_SUPPORT_STAGE_ROOT}/StemTeX/runtime/texmf-var/fonts/cache" "${STEMTEX_SUPPORT_STAGE_ROOT}")
        scholia_remove_inside("${STEMTEX_SUPPORT_STAGE_ROOT}/StemTeX/runtime/texmf-var/cache-warmup-renders" "${STEMTEX_SUPPORT_STAGE_ROOT}")
        scholia_remove_inside("${STEMTEX_SUPPORT_STAGE_ROOT}/StemTeX/runtime/texmf-var/cache-warmup-state" "${STEMTEX_SUPPORT_STAGE_ROOT}")
        file(REMOVE "${STEMTEX_SUPPORT_STAGE_ROOT}/StemTeX/runtime/texmf-var/xdvipdfmx-init-trace.log")
    endif()
else()
    scholia_remove_inside("${STEMTEX_SUPPORT_STAGE_ROOT}" "${WORKSPACE_ROOT}")
    message(WARNING "StemTeX TeX tree was not found under ${INSTALL_PREFIX}/StemTeX/runtime/texmf-dist.")
endif()

foreach(_required IN ITEMS
    "bin/scholia.exe"
    "bin/scholia.ico"
    "bin/data/applications/org.jairy.scholia.desktop"
    "bin/data/metainfo/org.jairy.scholia.appdata.xml"
    "bin/data/icons/hicolor/256x256/apps/scholia.png"
    "bin/data/scholia/tools.xml"
    "bin/data/scholia/toolsQuick.xml"
    "bin/data/scholia/drawingtools.xml"
    "bin/data/scholia/pics/annotation-latex-note.svg"
    "bin/data/locale/zh_CN/LC_MESSAGES/okular.mo"
    "share/poppler/cMap/Adobe-GB1/UniGB-UTF16-H"
    "share/poppler/cidToUnicode/Adobe-GB1"
)
    scholia_validate_file("${STAGE_ROOT}" "${_required}")
endforeach()

if(DEFINED SKIP_INSTALLER AND SKIP_INSTALLER)
    message(STATUS "Skipping installer build.")
    return()
endif()

file(MAKE_DIRECTORY "${OUTPUT_DIR}")
scholia_find_inno_setup(_iscc)
set(_iss "${SOURCE_ROOT}/windows-build/installer/scholia-installer.iss")
set(_support_iss "${SOURCE_ROOT}/windows-build/installer/scholia-stemtex-support.iss")

message(STATUS "Building installer with Inno Setup")
message(STATUS "  ISCC        : ${_iscc}")
message(STATUS "  Script      : ${_iss}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        "SCHOLIA_STAGE=${STAGE_ROOT}"
        "SCHOLIA_OUTPUT=${OUTPUT_DIR}"
        "SCHOLIA_VERSION=${VERSION}"
        "SCHOLIA_FILE_VERSION=${FILE_VERSION}"
        "${_iscc}" "${_iss}"
    RESULT_VARIABLE _iscc_result
)
if(NOT _iscc_result EQUAL 0)
    message(FATAL_ERROR "Inno Setup failed with exit code ${_iscc_result}")
endif()

if(_has_stemtex_support AND NOT (DEFINED SKIP_STEMTEX_SUPPORT_INSTALLER AND SKIP_STEMTEX_SUPPORT_INSTALLER))
    message(STATUS "Building StemTeX support installer with Inno Setup")
    message(STATUS "  Script      : ${_support_iss}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env
            "SCHOLIA_SUPPORT_STAGE=${STEMTEX_SUPPORT_STAGE_ROOT}"
            "SCHOLIA_OUTPUT=${OUTPUT_DIR}"
            "SCHOLIA_VERSION=${VERSION}"
            "SCHOLIA_FILE_VERSION=${FILE_VERSION}"
            "${_iscc}" "${_support_iss}"
        RESULT_VARIABLE _support_iscc_result
    )
    if(NOT _support_iscc_result EQUAL 0)
        message(FATAL_ERROR "StemTeX support Inno Setup failed with exit code ${_support_iscc_result}")
    endif()
endif()
