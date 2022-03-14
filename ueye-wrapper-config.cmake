# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
# 
# Copyright (C) 2020, Arne Wendt
#


#
# uEye-wrapper-config.cmake	-	package configuration file for iDS uEye
#								SDK C++ wrapper
#
#
# exports:
# 		uEye-wrapper -	target to link against; including all necessary
#						include directories
#


# TODO: installable library

IF(NOT TARGET ueye-wrapper)
	
	enable_language(CXX)
	find_package( uEye-SDK 4.94 REQUIRED )
	find_package( Threads REQUIRED )
	find_package( indicators REQUIRED )
	find_package( fmt REQUIRED )
	find_package( selene REQUIRED )
	
	include("${CMAKE_CURRENT_LIST_DIR}/uEye-wrapperTargets.cmake")

	# target_include_directories( uEye-wrapper INTERFACE ${OpenCV_INCLUDE_DIRS} )
ENDIF()
