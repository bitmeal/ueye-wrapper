# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
# 
# Copyright (C) 2020, Arne Wendt
#

IF(NOT CMAKE_PROJECT_NAME)
	cmake_minimum_required(VERSION 3.12.0)
	project(ueye-tools VERSION 0.1.0)

	SET(CMAKE_CXX_STANDARD 11)
ENDIF()


IF(NOT TARGET ueye-wrapper)
	find_package( ueye-wrapper REQUIRED )
ENDIF()
find_package( argh REQUIRED )


add_executable(ueye-ip-settings ueye_ip_settings.cpp )
target_link_libraries( ueye-ip-settings ueye-wrapper argh )


IF(WITH_TOOLS)
	SET(INSTALL_TOOLS TRUE CACHE BOOL "install uEye tools")
ENDIF()

IF(NOT (WITH_TOOLS AND NOT INSTALL_TOOLS))

	install(FILES "${CMAKE_CURRENT_LIST_DIR}/../LICENSE" DESTINATION .)
	install(FILES "${CMAKE_CURRENT_LIST_DIR}/../README.MD" DESTINATION .)
	install(TARGETS ueye-ip-settings RUNTIME DESTINATION bin)

ENDIF()