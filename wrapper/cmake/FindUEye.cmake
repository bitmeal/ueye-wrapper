# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
# 
# Copyright (C) 2020, Arne Wendt
#


#
# FindUEye.cmake - CMake module to find uEye library and include files
# and define the "UEye" library-target to link against
#


IF(NOT LIBUEYE_FOUND)

    IF(WIN32) # is Windows
        IF(CMAKE_SIZEOF_VOID_P MATCHES "8")
            SET(LIB_ARCH_SUFFIX "_64")
        ELSE()
            SET(LIB_ARCH_SUFFIX "")
        ENDIF()
		
	SET(PFX86 "PROGRAMFILES(X86)")
        # Be able to define a "common-root"-path to the ueye library and 
        # headers. Eases finding it in unusual locations. Try to determine
        # the LIBUEye_DIR based on the location of the header file
        FIND_PATH(UEye_DIR
            NAMES   include/uEye.h
            PATHS   "$ENV{PROGRAMFILES}/IDS/uEye/Develop/"
                    "$ENV{${PFX86}}/IDS/uEye/Develop/"
                    "$ENV{PROGRAMFILESW6432}/IDS/uEye/Develop/"
        )

        FIND_PATH(LIBUEYE_INCLUDE_DIRS
            NAMES   ueye.h
            PATHS   ${UEye_DIR}/include/
        )

        FIND_FILE(LIBUEYE_LIBRARIES "uEye_api${LIB_ARCH_SUFFIX}.lib"
            PATHS   ${UEye_DIR}/Lib/
        )

    ELSE() # is *NIX like

        FIND_PATH(UEye_DIR
            NAMES   include/ueye.h
            PATHS   /usr/
                    /usr/local/
        )

        FIND_PATH(LIBUEYE_INCLUDE_DIRS
            NAMES   ueye.h
            PATHS   ${UEye_DIR}
                    ${UEye_DIR}/include/
        )

        FIND_FILE(LIBUEYE_LIBRARIES libueye_api.so
            PATHS   ${UEye_DIR}
                    ${UEye_DIR}/lib/
                    ${UEye_DIR}/lib64/
                    ${UEye_DIR}/i386-linux-gnu/
                    ${UEye_DIR}/x86_64-linux-gnu/
        )

    ENDIF() # end plattform specific code


    IF(LIBUEYE_INCLUDE_DIRS AND LIBUEYE_LIBRARIES)
        IF(UEye_DIR)
            # hide individual variables if "common-root" is found
            MARK_AS_ADVANCED(LIBUEYE_INCLUDE_DIRS)
            MARK_AS_ADVANCED(LIBUEYE_LIBRARIES)
        ELSE()
            # hide "common-root" if not found, but individual variables are OK
            MARK_AS_ADVANCED(UEye_DIR)
        ENDIF()

        # FOUND EVERYTHING WE NEED
        SET (LIBUEYE_FOUND TRUE)
        # SETUP IMPORTED LIBRARY TARGET
        ADD_LIBRARY(uEye STATIC IMPORTED)
        set_target_properties(uEye PROPERTIES IMPORTED_LOCATION ${LIBUEYE_LIBRARIES})
        TARGET_INCLUDE_DIRECTORIES(uEye INTERFACE ${LIBUEYE_INCLUDE_DIRS})

    ELSE()
        # if individual variables are not OK, but UEye_DIR isn't either,
        # hide them. Try to keep configuration simple for the user
        MARK_AS_ADVANCED(LIBUEYE_INCLUDE_DIRS)
        MARK_AS_ADVANCED(LIBUEYE_LIBRARIES)
    ENDIF()

    IF(LIBUEYE_FOUND)
        MESSAGE(STATUS "Found uEye: ${LIBUEYE_INCLUDE_DIRS}, ${LIBUEYE_LIBRARIES}")
    ELSE()
        IF(LIBUEYE_FIND_REQUIRED)
            MESSAGE(FATAL_ERROR "Could not find uEye, try to setup UEye_DIR accordingly. Or, enable advanced mode and point to the include directory(s) and library(s) manually, using LIBUEYE_INCLUDE_DIRS and LIBUEYE_LIBRARIES.")
        ELSE()
            MESSAGE(STATUS "uEye not found.")
        ENDIF()
    ENDIF()
ENDIF()