/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 * 
 * Copyright (C) 2020, Arne Wendt
 * 
 * */


#pragma once

#include "wrapper_types.h"
#include "ueye_handle.h"

namespace uEyeWrapper
{
    cameraList getCameraList();
    
    // async only for now
    // template<typename T>
    // uEyeHandle<T> openCamera(const uEyeCameraInfo&, );

    template<colorMode T>
    uEyeHandle<T> openCamera(const uEyeCameraInfo, std::function<void(int, std::string)> = nullptr, std::function<void(uEyeCameraInfo, std::chrono::milliseconds, progress_state &)> = uploadProgressHandlerBar);
}