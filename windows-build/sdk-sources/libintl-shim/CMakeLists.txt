cmake_minimum_required(VERSION 3.22)

project(libintl-shim VERSION 0.1 LANGUAGES C)

add_library(intl SHARED libintl-shim.c)
target_compile_definitions(intl PRIVATE BUILDING_LIBINTL_SHIM)
target_include_directories(intl PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<INSTALL_INTERFACE:include>"
)
set_target_properties(intl PROPERTIES
    OUTPUT_NAME intl
    WINDOWS_EXPORT_ALL_SYMBOLS OFF
)

install(TARGETS intl
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
install(FILES libintl.h DESTINATION include)

add_executable(msgfmt msgfmt-shim.c)
add_executable(msgmerge msgmerge-shim.c)
install(TARGETS msgfmt msgmerge RUNTIME DESTINATION bin)
