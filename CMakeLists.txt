 # This Source Code Form is subject to the terms of the Mozilla Public
 # License, v. 2.0. If a copy of the MPL was not distributed with this
 # file, You can obtain one at https://mozilla.org/MPL/2.0/.
 # 
 # Copyright (C) 2020, Arne Wendt
 #


cmake_minimum_required(VERSION 3.0.0)
project(ueye_wrapper_proj VERSION 0.1.0)

SET(CMAKE_CXX_STANDARD 11)


add_subdirectory(wrapper)

add_subdirectory(tools)
add_subdirectory(examples)
