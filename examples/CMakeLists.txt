# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
# 
# Copyright (C) 2020, Arne Wendt
#


IF(NOT CMAKE_PROJECT_NAME)
	cmake_minimum_required(VERSION 3.12.0)
	project(ueye-wrapper VERSION 0.1.0)

	SET(CMAKE_CXX_STANDARD 11)
ENDIF()


IF(NOT TARGET ueye-wrapper)
	find_package( ueye-wrapper REQUIRED )
ENDIF()
find_package( argh REQUIRED )
find_package( OpenCV REQUIRED )


add_executable(ueye-multi ueye_multi.cpp )
target_include_directories( ueye-multi PRIVATE ${CMAKE_CURRENT_LIST_DIR}  ${OpenCV_INCLUDE_DIRS} )
target_link_libraries( ueye-multi ueye-wrapper  ${OpenCV_LIBS} )

add_executable(ueye-single ueye_single.cpp )
target_include_directories( ueye-single PRIVATE ${CMAKE_CURRENT_LIST_DIR}  ${OpenCV_INCLUDE_DIRS} )
target_link_libraries( ueye-single ueye-wrapper  ${OpenCV_LIBS} argh )

IF(WITH_EXAMPLES AND WIN32)
	SET(INSTALL_EXAMPLES TRUE CACHE BOOL "install ueye-wrapper examples")
	SET(INSTALL_EXAMPLES_SRC TRUE CACHE BOOL "install ueye-wrapper example sources")
ENDIF()


# export example sources to build directory
file(GLOB EXAMPLE_SOURCES "${CMAKE_CURRENT_LIST_DIR}/*.cpp")
foreach(EXAMPLE_SOURCE_FILE ${EXAMPLE_SOURCES})
	configure_file(${EXAMPLE_SOURCE_FILE} src/ COPYONLY)
endforeach()
configure_file("${CMAKE_CURRENT_LIST_DIR}/examples-CMakeLists.txt" src/CMakeLists.txt COPYONLY)


# prepare cross plattform install paths
IF(WIN32) # is Windows
	SET(CMAKE_INSTALL_INCLUDEDIR "${CMAKE_INSTALL_PREFIX}/include")
	SET(CMAKE_INSTALL_CMAKEDIR "${CMAKE_INSTALL_PREFIX}/CMake")
	SET(CMAKE_INSTALL_LIBDIR "${CMAKE_INSTALL_PREFIX}/lib")
	SET(CMAKE_INSTALL_BINDIR "${CMAKE_INSTALL_PREFIX}/bin")
	SET(CMAKE_INSTALL_DOCDIR "${CMAKE_INSTALL_PREFIX}")
	SET(CMAKE_INSTALL_EXAMPLESDIR "${CMAKE_INSTALL_PREFIX}")
ELSE() # is *NIX like
	include(GNUInstallDirs)
	SET(CMAKE_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/${CMAKE_PROJECT_NAME}")
ENDIF() # end plattform specific code

IF(NOT (WITH_EXAMPLES AND NOT INSTALL_EXAMPLES) AND WIN32)

	install(FILES "${CMAKE_CURRENT_LIST_DIR}/../LICENSE" DESTINATION "${CMAKE_INSTALL_DOCDIR}")
	install(FILES "${CMAKE_CURRENT_LIST_DIR}/../README.MD" DESTINATION "${CMAKE_INSTALL_DOCDIR}")
	
	install(TARGETS
			ueye-multi
			ueye-single
			RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/examples/bin")

ENDIF()

IF(NOT (WITH_EXAMPLES AND NOT INSTALL_EXAMPLES_SRC))
	
	install(FILES "${CMAKE_CURRENT_LIST_DIR}/../LICENSE" DESTINATION "${CMAKE_INSTALL_DOCDIR}")
	install(FILES "${CMAKE_CURRENT_LIST_DIR}/../README.MD" DESTINATION "${CMAKE_INSTALL_DOCDIR}")
	
	install(FILES "${CMAKE_CURRENT_LIST_DIR}/examples-CMakeLists.txt" DESTINATION "${CMAKE_INSTALL_PREFIX}/examples/src" RENAME CMakeLists.txt)
	install(DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/" DESTINATION "${CMAKE_INSTALL_PREFIX}/examples/src" FILES_MATCHING PATTERN "*.cpp")

ENDIF()
