
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_BINARY_DIR}/../
)

# scholia

set(scholia_SRCS
   main.cpp
   okular_main.cpp
   shell.cpp
   shellutils.cpp
   welcomescreen.cpp

   ../gui/recentitemsmodel.cpp

   shell.qrc
)

if(WIN32)
    list(APPEND scholia_SRCS ../icons/scholia.rc)
endif()

ki18n_wrap_ui(scholia_SRCS
    welcomescreen.ui)

file(GLOB ICONS_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/../icons/*-apps-scholia.png")
ecm_add_app_icon(scholia_SRCS ICONS ${ICONS_SRCS})

add_executable(scholia ${scholia_SRCS})

target_link_libraries(scholia KF6::I18n KF6::Parts KF6::WindowSystem KF6::Crash KF6::IconThemes KF6::KIOGui)
if (TARGET Qt6::DBus)
    target_link_libraries(scholia Qt6::DBus)
endif()
if (HAVE_X11)
    target_link_libraries(scholia Qt6::GuiPrivate)
endif()

install(TARGETS scholia ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

if(APPLE)
    set_target_properties(scholia PROPERTIES
        MACOSX_BUNDLE_DISPLAY_NAME "Scholia"
        MACOSX_BUNDLE_BUNDLE_NAME "Scholia"
        MACOSX_BUNDLE_ICON_FILE "scholia_SRCS"
        MACOSX_BUNDLE_LONG_VERSION_STRING "Scholia ${SCHOLIA_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${SCHOLIA_VERSION_MAJOR}.${SCHOLIA_VERSION_MINOR}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${SCHOLIA_VERSION}"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/MacOSXBundleInfo.plist.in"
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.jairy.scholia"
        MACOSX_BUNDLE_COPYRIGHT "2026 The Scholia Contributors; 2002-2026 The Okular Developers")
endif()

# Scholia shell data files

install( PROGRAMS org.jairy.scholia.desktop  DESTINATION  ${KDE_INSTALL_APPDIR} )
install( FILES org.jairy.scholia.appdata.xml DESTINATION  ${KDE_INSTALL_METAINFODIR} )
