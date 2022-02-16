# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
# 
# Copyright (C) 2020, Arne Wendt
#


#
# FindUEYE-SDK.cmake	- CMake module to find iDS uEye SDK
# UEYE-SDK-config.cmake	- package configuration file for iDS uEye SDK
#
#
# exports:
# 		ueye-sdk - as IMPORTED library target with its INTERFACE_INCLUDE_DIRECTORIES set
#
# sets:
# 		UEYE-SDK_INCLUDE_DIRS	- the directory containing uEye.h/ueye.h
#		UEYE-SDK_LIBRARIES		- uEye SDK libraries
#



IF(NOT TARGET ueye-sdk)
IF(NOT UEYE-SDK_FOUND)

	IF(WIN32) # is Windows
		IF(CMAKE_SIZEOF_VOID_P MATCHES "8")
			SET(LIB_ARCH_SUFFIX "_64")
		ELSE()
			SET(LIB_ARCH_SUFFIX "")
		ENDIF()
		
		# assign PROGRAMFILES(X86) name to variable, as variable expansion
		# with typed out "PROGRAMFILES(X86)" fails
		SET(PFX86 "PROGRAMFILES(X86)")

		# visual studios PROGRAMFILES variable may point to a different
		# directory than the standard environment; PROGRAMFILESW6432 seem
		# to be missing completely. try to point to the non-x86 dir by
		# stripping " (x86)" from the end of PROGRAMFILES
		STRING(REGEX REPLACE " \\([xX]86\\).*" "" PFNX86 $ENV{PROGRAMFILES})

		# Be able to define a "common-root"-path to the ueye library and 
		# headers. Eases finding it in unusual locations. Try to determine
		# the LIBUEye_DIR based on the location of the header file
		FIND_PATH(UEYE-SDK
			NAMES   include/uEye.h
			PATHS   "$ENV{PROGRAMFILES}"
					"$ENV{${PFX86}}"
					"$ENV{PROGRAMFILESW6432}"
					"${PFNX86}"
			PATH_SUFFIXES
					"IDS/uEye/Develop/"

		)

		FIND_PATH(UEYE-SDK_INCLUDE_DIRS
			NAMES   uEye.h
			PATHS   ${UEYE-SDK}/include/
		)

		FIND_FILE(UEYE-SDK_LIBRARIES "uEye_api${LIB_ARCH_SUFFIX}.lib"
			PATHS   ${UEYE-SDK}/Lib/
		)

	ELSE() # is *NIX like

		FIND_PATH(UEYE-SDK
			NAMES   include/ueye.h
			PATHS   /usr/
					/usr/local/
		)

		FIND_PATH(UEYE-SDK_INCLUDE_DIRS
			NAMES   ueye.h
			PATHS   ${UEYE-SDK}
					${UEYE-SDK}/include/
		)

		FIND_FILE(UEYE-SDK_LIBRARIES libueye_api.so
			PATHS   ${UEYE-SDK}
			PATH_SUFFIXES	"lib/"
							"lib64/"
							"i386-linux-gnu/"
							"x86_64-linux-gnu/"
		)

	ENDIF() # end plattform specific code


	IF(UEYE-SDK_INCLUDE_DIRS AND UEYE-SDK_LIBRARIES)
		IF(UEYE-SDK)
			# hide individual variables if "common-root" is found
			MARK_AS_ADVANCED(UEYE-SDK_INCLUDE_DIRS)
			MARK_AS_ADVANCED(UEYE-SDK_LIBRARIES)
		ELSE()
			# hide "common-root" if not found, but individual variables are OK
			MARK_AS_ADVANCED(UEYE-SDK)
		ENDIF()

		# FOUND EVERYTHING WE NEED
		SET (UEYE-SDK_FOUND TRUE)
		# SETUP IMPORTED LIBRARY TARGET
		ADD_LIBRARY(ueye-sdk STATIC IMPORTED)
		SET_TARGET_PROPERTIES(ueye-sdk PROPERTIES IMPORTED_LOCATION ${UEYE-SDK_LIBRARIES})
		TARGET_INCLUDE_DIRECTORIES(ueye-sdk INTERFACE ${UEYE-SDK_INCLUDE_DIRS})

	ELSE()
		# if individual variables are not OK, but UEYE-SDK isn't either,
		# hide them. Try to keep configuration simple for the user
		MARK_AS_ADVANCED(UEYE-SDK_INCLUDE_DIRS)
		MARK_AS_ADVANCED(UEYE-SDK_LIBRARIES)
	ENDIF()

	IF(UEYE-SDK_FOUND)
		MESSAGE(STATUS "Found ueye-sdk: ${UEYE-SDK_INCLUDE_DIRS}, ${UEYE-SDK_LIBRARIES}")
	ELSE()
		IF(UEYE-SDK_FIND_REQUIRED)
			MESSAGE(FATAL_ERROR "Could not find ueye-sdk, try to setup UEYE-SDK accordingly. Or, enable advanced mode and point to the include directory(s) and library(s) manually, using UEYE-SDK_INCLUDE_DIRS and UEYE-SDK_LIBRARIES.")
		ELSE()
			MESSAGE(STATUS "ueye-sdk not found.")
		ENDIF()
	ENDIF()
ENDIF()
ENDIF()
