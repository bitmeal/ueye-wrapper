# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
# 
# Copyright (C) 2020, Arne Wendt
#

IF(CMAKE_INSTALL_PREFIX AND WIN32) #this CMakeLists is included by a parent project and we are on Windows
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


# prepare cross plattform install paths
IF(WIN32) # is Windows
	SET(CMAKE_INSTALL_INCLUDEDIR "${CMAKE_INSTALL_PREFIX}/include")
	SET(CMAKE_INSTALL_CMAKEDIR "${CMAKE_INSTALL_PREFIX}/CMake")
	SET(CMAKE_INSTALL_LIBDIR "${CMAKE_INSTALL_PREFIX}/lib")
	SET(CMAKE_INSTALL_BINDIR "${CMAKE_INSTALL_PREFIX}/bin")
	SET(CMAKE_INSTALL_DOCDIR "${CMAKE_INSTALL_PREFIX}")
ELSE() # is *NIX like
	include(GNUInstallDirs)
	SET(CMAKE_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/${CMAKE_PROJECT_NAME}")
ENDIF() # end plattform specific code

IF(NOT (WITH_UEYE-SDK-CMAKE AND NOT INSTALL_UEYE-SDK-CMAKE))
	install(FILES "${CMAKE_CURRENT_LIST_DIR}/../LICENSE" DESTINATION "${CMAKE_INSTALL_DOCDIR}")
	install(FILES "${CMAKE_CURRENT_LIST_DIR}/../README.MD" DESTINATION "${CMAKE_INSTALL_DOCDIR}")
	install(FILES "${CMAKE_CURRENT_LIST_DIR}/FindUEYE-SDK.cmake" DESTINATION "${CMAKE_INSTALL_CMAKEDIR}" RENAME UEYE-SDK-config.cmake)
ENDIF()
