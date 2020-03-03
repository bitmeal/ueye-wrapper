# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
# 
# Copyright (C) 2020, Arne Wendt
#


cmake_minimum_required(VERSION 3.0.0)
project(ueye_wrapper_examples VERSION 0.1.0)

SET(CMAKE_CXX_STANDARD 11)

find_package( argh REQUIRED )


add_executable(ueye_ip_settings ueye_ip_settings.cpp )
#target_include_directories( ueye_ip_settings PRIVATE ${CMAKE_CURRENT_LIST_DIR} )
target_link_libraries( ueye_ip_settings ueye_wrapper argh )
