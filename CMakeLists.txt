cmake_minimum_required(VERSION 3.10)

project(fcitx5-lotus VERSION 1.2.0)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/lotus-version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/lotus-version.h"
    @ONLY
)

include_directories("${CMAKE_CURRENT_BINARY_DIR}")

set(REQUIRED_FCITX_VERSION 5.0.5)
find_package(ECM 1.0.0 REQUIRED)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
include(FeatureSummary)
include(GNUInstallDirs)
include(ECMUninstallTarget)

option(ENABLE_QT "Enable Qt based keymap editor" On)

find_package(Fcitx5Core ${REQUIRED_FCITX_VERSION} REQUIRED)
find_package(Pthread REQUIRED)
find_package(Gettext REQUIRED)
find_package(X11 REQUIRED)

include("${FCITX_INSTALL_CMAKECONFIG_DIR}/Fcitx5Utils/Fcitx5CompilerSettings.cmake")
add_definitions(-DFCITX_GETTEXT_DOMAIN=\"fcitx5-lotus\") 

find_package(Fcitx5Utils REQUIRED)
if (Fcitx5Utils_VERSION VERSION_GREATER_EQUAL "5.1.13")
    add_definitions(-DLOTUS_USE_MODERN_FCITX_API)
    message(STATUS "Fcitx5 version ${Fcitx5Utils_VERSION} detected: Using modern API")
else()
    message(STATUS "Fcitx5 version ${Fcitx5Utils_VERSION} detected: Using legacy API")
endif()
fcitx5_add_i18n_definition()

add_subdirectory(po)
add_subdirectory(bamboo)
add_subdirectory(src)
add_subdirectory(data)
add_subdirectory(server)
add_subdirectory(misc)

if(ENABLE_QT)
    message(STATUS "QT is enabled, adding keymap editor GUI")
    target_compile_definitions(lotus PRIVATE ENABLE_KEYMAP_EDITOR)
    set(QT_MAJOR_VERSION 6)
    find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
    find_package(Fcitx5Qt${QT_MAJOR_VERSION}WidgetsAddons 5.0.10 REQUIRED)
    include_directories("${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_subdirectory(keymap-editor)
endif(ENABLE_QT)

fcitx5_translate_desktop_file(org.fcitx.Fcitx5.Addon.Lotus.metainfo.xml.in
    org.fcitx.Fcitx5.Addon.Lotus.metainfo.xml XML)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/org.fcitx.Fcitx5.Addon.Lotus.metainfo.xml" DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)

install(FILES 
        LICENSES/GPL-3.0-or-later.txt
        LICENSES/LGPL-2.1-or-later.txt
        DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/${PROJECT_NAME}
)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)