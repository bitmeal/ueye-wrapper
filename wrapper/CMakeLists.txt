# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
# 
# Copyright (C) 2020, Arne Wendt
#


cmake_minimum_required(VERSION 3.12.0)
project(ueye-wrapper VERSION 0.1.0)

SET(CMAKE_CXX_STANDARD 11)


find_package( UEYE-SDK REQUIRED )
find_package( Threads REQUIRED )
find_package( OpenCV REQUIRED )


add_library( ueye-wrapper src/ueye_wrapper.cpp )
target_include_directories( ueye-wrapper PUBLIC	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include> )
target_include_directories( ueye-wrapper PUBLIC	$<BUILD_INTERFACE:${OpenCV_INCLUDE_DIRS}> )
target_link_libraries( ueye-wrapper ueye-sdk opencv_core Threads::Threads )


IF(WITH_WRAPPER)
	SET(INSTALL_WRAPPER TRUE CACHE BOOL "install uEye C++ wrapper")
ENDIF()

IF(NOT (WITH_WRAPPER AND NOT INSTALL_WRAPPER))

	install(TARGETS ueye-wrapper
			EXPORT ueye-wrapperTargets
			ARCHIVE DESTINATION lib
			LIBRARY DESTINATION lib
			INCLUDES DESTINATION include
			)
	install(FILES "${CMAKE_CURRENT_LIST_DIR}/include/ueye_wrapper.h" DESTINATION include)
	install(FILES "${CMAKE_CURRENT_LIST_DIR}/include/ip_helpers.h" DESTINATION include)
	install(FILES "${CMAKE_CURRENT_LIST_DIR}/../LICENSE" DESTINATION .)
	install(FILES "${CMAKE_CURRENT_LIST_DIR}/../README.MD" DESTINATION .)
	install(FILES "${CMAKE_CURRENT_LIST_DIR}/ueye-wrapper-config.cmake" DESTINATION CMake)
	install(EXPORT ueye-wrapperTargets DESTINATION CMake)

ENDIF()
