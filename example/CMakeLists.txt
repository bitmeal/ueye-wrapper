 # This Source Code Form is subject to the terms of the Mozilla Public
 # License, v. 2.0. If a copy of the MPL was not distributed with this
 # file, You can obtain one at https://mozilla.org/MPL/2.0/.
 # 
 # Copyright (C) 2020, Arne Wendt
 #


 cmake_minimum_required(VERSION 3.12.0)
 SET(CMAKE_CXX_STANDARD 17)
 
 # fetch vcpkg
 set(VCPKG_VERSION edge)
 include("${CMAKE_CURRENT_LIST_DIR}/../external/vcpkg-cmake-integration/vcpkg.cmake")
 
 project(uEye-wrapper-examples VERSION 0.1.0)
 
 # fetch/install dependencies
 SET(FETCH_DEPENDENCIES TRUE CACHE BOOL "fetch dependencies locally")
 IF(FETCH_DEPENDENCIES)	
     vcpkg_add_package(fmt)
     vcpkg_add_package(plog)
    #  vcpkg_add_package(selene[opencv])
     vcpkg_add_package(selene)
     vcpkg_add_package(indicators)
     vcpkg_add_package(bshoshany-thread-pool)
 ENDIF()
 
 # include wrapper
 add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/.." "${CMAKE_CURRENT_LIST_DIR}/build/wrapper")
 
 # build examples
 add_executable(uEye-trigger "${CMAKE_CURRENT_LIST_DIR}/trigger.cpp")
 target_link_libraries(uEye-trigger uEye-wrapper)