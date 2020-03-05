# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
# 
# Copyright (C) 2020, Arne Wendt
#

IF(CMAKE_INSTALL_PREFIX)
	GET_FILENAME_COMPONENT(CMAKE_INSTALL_PREFIX_BASE "${CMAKE_INSTALL_PREFIX}/.." ABSOLUTE)
	SET(CMAKE_INSTALL_PREFIX_UEYE-SDK "${CMAKE_INSTALL_PREFIX_BASE}/UEYE-SDK" CACHE PATH "Install path prefix, prepended onto install directories for UEYE-SDK CMake files")
	
	IF(CMAKE_INSTALL_PREFIX_CACHED AND NOT (CMAKE_INSTALL_PREFIX_CACHED STREQUAL CMAKE_INSTALL_PREFIX))
		# if parent project install prefix has been modified, force override on our install prefix
		SET(CMAKE_INSTALL_PREFIX_UEYE-SDK "${CMAKE_INSTALL_PREFIX_BASE}/UEYE-SDK" CACHE PATH "Install path prefix, prepended onto install directories for UEYE-SDK CMake files" FORCE)
	ENDIF()

	SET(CMAKE_INSTALL_PREFIX_CACHED ${CMAKE_INSTALL_PREFIX} CACHE INTERNAL "last install prefix from parent project - INTERNAL")

	SET(CMAKE_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX_UEYE-SDK})
ENDIF()

cmake_minimum_required(VERSION 3.12.0)
project(UEYE-SDK VERSION 0.1.0)

SET(CMAKE_CXX_STANDARD 11)

IF(WITH_UEYE-SDK-CMAKE)
	SET(INSTALL_UEYE-SDK-CMAKE TRUE CACHE BOOL "install CMake files to find the iDS uEye SDK with CMakes find_package(UEye)")
ENDIF()

IF(NOT (WITH_UEYE-SDK-CMAKE AND NOT INSTALL_UEYE-SDK-CMAKE))
	MESSAGE(STATUS "CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")
	install(FILES "${CMAKE_CURRENT_LIST_DIR}/../LICENSE" DESTINATION "${CMAKE_INSTALL_PREFIX}")
	install(FILES "${CMAKE_CURRENT_LIST_DIR}/../README.MD" DESTINATION "${CMAKE_INSTALL_PREFIX}")
	install(FILES "${CMAKE_CURRENT_LIST_DIR}/FindUEYE-SDK.cmake" DESTINATION "${CMAKE_INSTALL_PREFIX}/CMake" RENAME UEYE-SDK-config.cmake)
ENDIF()
