 # This Source Code Form is subject to the terms of the Mozilla Public
 # License, v. 2.0. If a copy of the MPL was not distributed with this
 # file, You can obtain one at https://mozilla.org/MPL/2.0/.
 # 
 # Copyright (C) 2020, Arne Wendt
 #


cmake_minimum_required(VERSION 3.12.0)
project(ueye-wrapper VERSION 0.1.0)

SET(CMAKE_CXX_STANDARD 17)

SET(WITH_UEYE-SDK-CMAKE TRUE CACHE BOOL "include and allow install of CMake files to find the iDS uEye SDK with CMakes find_package(UEye)")
if(WITH_UEYE-SDK-CMAKE)
	add_subdirectory(ueye-sdk-cmake)
	# check whether to include our local FindUEye.cmake file 
	find_package(UEYE-SDK QUIET)
	if(NOT UEYE-SDK_FOUND)
		list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/ueye-sdk-cmake")
	endif()
endif()

SET(WITH_WRAPPER TRUE CACHE BOOL "build uEye SDK C++ wrapper")
if(WITH_WRAPPER)
	add_subdirectory(wrapper)
endif()

SET(WITH_TOOLS TRUE CACHE BOOL "build tools for uEye cameras")
if(WITH_TOOLS)
	add_subdirectory(tools)
endif()

SET(WITH_EXAMPLES TRUE CACHE BOOL "build example applications")
if(WITH_EXAMPLES)
	add_subdirectory(examples)
endif()
