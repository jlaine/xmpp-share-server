cmake_minimum_required(VERSION 2.6)

# Project and version
project(xmpp-share-server)
set(SERVER_VERSION_MAJOR 0)
set(SERVER_VERSION_MINOR 8)
set(SERVER_VERSION_PATCH 10)
set(SERVER_VERSION ${SERVER_VERSION_MAJOR}.${SERVER_VERSION_MINOR}.${SERVER_VERSION_PATCH})
set(SERVER_PLUGIN_DIR lib/xmpp-share-server)
configure_file(${CMAKE_SOURCE_DIR}/src/config.h.in ${CMAKE_BINARY_DIR}/src/config.h)

# Required
find_package(Qt4 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(QDJANGO qdjango-db qdjango-http REQUIRED)
pkg_check_modules(QXMPP qxmpp REQUIRED)

set(PLUGIN_INSTALL_DIR lib/qt4/plugins)
add_subdirectory(src)

# Packaging
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "xmpp-share-server")
set(CPACK_PACKAGE_VERSION_MAJOR ${SERVER_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${SERVER_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${SERVER_VERSION_PATCH})

# Source package
set(CPACK_SOURCE_GENERATOR "TBZ2")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${PROJECT_NAME}-${SERVER_VERSION}")
list(APPEND CPACK_SOURCE_IGNORE_FILES "/.git/")
list(APPEND CPACK_SOURCE_IGNORE_FILES "/.gitmodules")
list(APPEND CPACK_SOURCE_IGNORE_FILES "${CMAKE_CURRENT_BINARY_DIR}")

include(CPack)
