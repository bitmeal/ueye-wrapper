# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
# 
# Copyright (C) 2020, Arne Wendt
#


#
# ueye-wrapper-config.cmake	-	package configuration file for iDS uEye
#								SDK C++ wrapper
#
#
# exports:
# 		ueye-wrapper -	target to link against; including all necessary
#						include directories
#
# sets:
# 		ueye-wrapper_INCLUDE_DIR -	the directory containing:
#									* ueye_wrapper.h
#									* ip_helpers.h
#



IF(NOT TARGET ueye-wrapper)
	
	enable_language(CXX)
	find_package( UEYE-SDK REQUIRED QUIET )
	find_package( Threads REQUIRED QUIET )
	find_package( OpenCV REQUIRED QUIET )

	include("${CMAKE_CURRENT_LIST_DIR}/ueye-wrapperTargets.cmake")
	
	target_include_directories( ueye-wrapper INTERFACE ${OpenCV_INCLUDE_DIRS} )
	get_target_property(ueye-wrapper_INCLUDE_DIR ueye-wrapper INTERFACE_INCLUDE_DIRECTORIES)

ENDIF()
