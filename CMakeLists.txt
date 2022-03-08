 # This Source Code Form is subject to the terms of the Mozilla Public
 # License, v. 2.0. If a copy of the MPL was not distributed with this
 # file, You can obtain one at https://mozilla.org/MPL/2.0/.
 # 
 # Copyright (C) 2020, Arne Wendt
 #


cmake_minimum_required(VERSION 3.12.0)
SET(CMAKE_CXX_STANDARD 17)

# fetch vcpkg
set(VCPKG_VERSION edge) # optional
include("${CMAKE_CURRENT_LIST_DIR}/vcpkg-cmake-integration/vcpkg.cmake")

project(uEye-wrapper VERSION 0.1.0)

# find uEye SDK
find_package(uEye-SDK QUIET)
if(NOT uEye-SDK_FOUND)
	list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/ueye-cmake")
endif()


# fetch/install dependencies
# TODO: disable by default
# TODO: install dependencies
SET(FETCH_DEPENDENCIES TRUE CACHE BOOL "fetch dependencies locally")
IF(FETCH_DEPENDENCIES)	
	vcpkg_add_package(fmt)
	vcpkg_add_package(plog)
	vcpkg_add_package(selene[opencv])
	vcpkg_add_package(indicators)
ENDIF()

# find/setup dependencies
find_package( uEye-SDK 4.95 REQUIRED )
find_package( Threads REQUIRED )
find_package( indicators REQUIRED )
find_path(PLOG_INCLUDE_DIRS "plog/Init.h" REQUIRED)
find_package( fmt REQUIRED )
find_package( selene REQUIRED )



# configure library
add_library( uEye-wrapper src/ueye_wrapper.cpp src/ueye_handle.cpp )

target_include_directories( uEye-wrapper PUBLIC	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include> )
target_include_directories( uEye-wrapper PUBLIC	$<BUILD_INTERFACE:${OpenCV_INCLUDE_DIRS}> )

target_link_libraries( uEye-wrapper uEye-SDK Threads::Threads fmt::fmt indicators::indicators selene::selene )
target_include_directories(uEye-wrapper PRIVATE ${PLOG_INCLUDE_DIRS})

target_compile_definitions(uEye-wrapper PRIVATE NOMINMAX)

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


SET(INSTALL_WRAPPER TRUE CACHE BOOL "install uEye C++ wrapper")
IF(INSTALL_WRAPPER)
	install(TARGETS uEye-wrapper
			EXPORT uEye-wrapperTargets
			ARCHIVE DESTINATION lib
			LIBRARY DESTINATION lib
			INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
			)
	install(FILES "${CMAKE_CURRENT_LIST_DIR}/include/ueye_wrapper.h" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
	install(FILES "${CMAKE_CURRENT_LIST_DIR}/include/ip_helpers.h" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
	install(FILES "${CMAKE_CURRENT_LIST_DIR}/../LICENSE" DESTINATION "${CMAKE_INSTALL_DOCDIR}")
	install(FILES "${CMAKE_CURRENT_LIST_DIR}/../README.MD" DESTINATION "${CMAKE_INSTALL_DOCDIR}")
	install(FILES "${CMAKE_CURRENT_LIST_DIR}/uEye-wrapper-config.cmake" DESTINATION "${CMAKE_INSTALL_CMAKEDIR}")
	install(EXPORT uEye-wrapperTargets DESTINATION "${CMAKE_INSTALL_CMAKEDIR}")
ENDIF()


# build dev demo
add_executable(dev "${CMAKE_CURRENT_LIST_DIR}/dev/dev.cpp")
target_link_libraries(dev uEye-wrapper)