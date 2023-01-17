 # This Source Code Form is subject to the terms of the Mozilla Public
 # License, v. 2.0. If a copy of the MPL was not distributed with this
 # file, You can obtain one at https://mozilla.org/MPL/2.0/.
 # 
 # Copyright (C) 2020, Arne Wendt
 #


cmake_minimum_required(VERSION 3.12.0)
SET(CMAKE_CXX_STANDARD 17)

project(uEye-wrapper VERSION 0.1.1)


# find/setup dependencies
	# setup: find uEye SDK
	find_package(uEye-SDK QUIET)
	if(NOT uEye-SDK_FOUND)
		list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/external/ueye-cmake")
	endif()

	# find dependencies
	find_package( uEye-SDK 4.94 REQUIRED )
	find_package( Threads REQUIRED )
	find_package( indicators REQUIRED )
	find_package( fmt REQUIRED )
	find_package( selene REQUIRED )

	## find_package(plog)
	find_path(PLOG_INCLUDE_DIRS "plog/Init.h")
	if(NOT PLOG_INCLUDE_DIRS)
	find_package(plog)
	if(NOT plog_FOUND)
		message(SEND_ERROR "could neither find PLOG_INCLUDE_DIRS (vcpkg style), nor plog library target using find_package(plog); plog is required")
	endif()
	endif()

	## find thread-pool
	find_path(BSHOSHANY_THREAD_POOL_INCLUDE_DIRS "BS_thread_pool.hpp" REQUIRED)
	if(NOT BSHOSHANY_THREAD_POOL_INCLUDE_DIRS) # ever being dispatched after "REQUIRED"?
		message(SEND_ERROR "could not find BSHOSHANY_THREAD_POOL_INCLUDE_DIRS; using vcpkg as per thread-pool documentation is recommended; bshoshany/thread-pool is required")
	endif()


# configure library
add_library( uEye-wrapper src/ueye_wrapper.cpp src/ueye_handle.cpp src/ueye_capture_handle.cpp )
	target_include_directories( uEye-wrapper PUBLIC	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include> )
	target_link_libraries( uEye-wrapper uEye-SDK Threads::Threads fmt::fmt indicators::indicators selene::selene )
	if(NOT PLOG_INCLUDE_DIRS)
		target_link_libraries( uEye-wrapper plog )
	else()
		target_include_directories(uEye-wrapper PRIVATE ${PLOG_INCLUDE_DIRS})
	endif()
	target_include_directories(uEye-wrapper PRIVATE ${BSHOSHANY_THREAD_POOL_INCLUDE_DIRS})
	# allow indicators to compile setting NOMINMAX
	target_compile_definitions(indicators::indicators INTERFACE NOMINMAX)
	# silence codecvt warnings from indicators
	target_compile_definitions(indicators::indicators INTERFACE _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING)
	# setting properties as in official iDS example
	target_compile_definitions(uEye-SDK INTERFACE NOMINMAX)
	set_target_properties(uEye-wrapper PROPERTIES C_STANDARD 17 CXX_STANDARD 17 C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

