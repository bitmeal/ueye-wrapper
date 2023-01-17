# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
# 
# Copyright (C) 2022, Arne Wendt
#

cmake_minimum_required(VERSION 3.12.0)
SET(CMAKE_CXX_STANDARD 17)

# make vcpkg automatically install dependencies on "make install" **experimental feature**
# X_VCPKG_APPLOCAL_DEPS_INSTALL depends on CMake policy CMP0087 and requires CMake 3.14
if(POLICY CMP0087)
    cmake_policy(SET CMP0087 NEW)
endif()
set(X_VCPKG_APPLOCAL_DEPS_INSTALL ON)

# fetch vcpkg
set(VCPKG_VERSION edge)
include("${CMAKE_CURRENT_LIST_DIR}/../external/vcpkg-cmake-integration/vcpkg.cmake")

project(uEye-wrapper-examples VERSION 0.1.1)

# include wrapper
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/.." "${CMAKE_CURRENT_LIST_DIR}/build/wrapper")

# build examples
add_executable(uEye-trigger "${CMAKE_CURRENT_LIST_DIR}/trigger.cpp")
target_link_libraries(uEye-trigger uEye-wrapper)

# prepare cross plattform install paths
IF(WIN32) # is Windows
    SET(CMAKE_INSTALL_INCLUDEDIR "${CMAKE_INSTALL_PREFIX}/include")
    SET(CMAKE_INSTALL_CMAKEDIR "${CMAKE_INSTALL_PREFIX}/CMake")
    SET(CMAKE_INSTALL_LIBDIR "${CMAKE_INSTALL_PREFIX}/lib")
    SET(CMAKE_INSTALL_BINDIR "${CMAKE_INSTALL_PREFIX}/bin")
    SET(CMAKE_INSTALL_DOCDIR "${CMAKE_INSTALL_PREFIX}")
ELSE() # is *NIX like
    include(GNUInstallDirs)
    SET(CMAKE_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")
ENDIF() # end plattform specific code

# install example
install(TARGETS uEye-trigger RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
