# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
# 
# Copyright (C) 2020, Arne Wendt
#


cmake_minimum_required(VERSION 3.12.0)
project(ueye-wrapper VERSION 0.1.0)

SET(CMAKE_CXX_STANDARD 17)


find_package( UEYE-SDK 4.95 REQUIRED )
find_package( Threads REQUIRED )
find_package( OpenCV REQUIRED )


add_library( ueye-wrapper src/ueye_wrapper.cpp )
target_include_directories( ueye-wrapper PUBLIC	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include> )
target_include_directories( ueye-wrapper PUBLIC	$<BUILD_INTERFACE:${OpenCV_INCLUDE_DIRS}> )
target_link_libraries( ueye-wrapper ueye-sdk opencv_core Threads::Threads )


IF(WITH_WRAPPER)
	SET(INSTALL_WRAPPER TRUE CACHE BOOL "install uEye C++ wrapper")
ENDIF()


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

IF(NOT (WITH_WRAPPER AND NOT INSTALL_WRAPPER))

	install(TARGETS ueye-wrapper
			EXPORT ueye-wrapperTargets
			ARCHIVE DESTINATION lib
			LIBRARY DESTINATION lib
			INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
			)
	install(FILES "${CMAKE_CURRENT_LIST_DIR}/include/ueye_wrapper.h" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
	install(FILES "${CMAKE_CURRENT_LIST_DIR}/include/ip_helpers.h" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
	install(FILES "${CMAKE_CURRENT_LIST_DIR}/../LICENSE" DESTINATION "${CMAKE_INSTALL_DOCDIR}")
	install(FILES "${CMAKE_CURRENT_LIST_DIR}/../README.MD" DESTINATION "${CMAKE_INSTALL_DOCDIR}")
	install(FILES "${CMAKE_CURRENT_LIST_DIR}/ueye-wrapper-config.cmake" DESTINATION "${CMAKE_INSTALL_CMAKEDIR}")
	install(EXPORT ueye-wrapperTargets DESTINATION "${CMAKE_INSTALL_CMAKEDIR}")

ENDIF()
