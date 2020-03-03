cmake_minimum_required(VERSION 3.0.0)
project(lib_ueye_wrapper VERSION 0.1.0)

SET(CMAKE_CXX_STANDARD 11)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
find_package(UEye REQUIRED )
FIND_PACKAGE( Threads REQUIRED )
find_package( OpenCV REQUIRED )

set(OpenCV_STATIC ON)

add_library( ueye_wrapper src/ueye_wrapper.cpp )
target_include_directories( ueye_wrapper PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include" ${OpenCV_INCLUDE_DIRS} )
target_link_libraries( ueye_wrapper uEye ${OpenCV_LIBS} Threads::Threads )
