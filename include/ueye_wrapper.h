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
    // TODO: find saner implementation; concurrency impacts handles not the wrapper
    extern size_t concurrency;
    
    
    cameraList getCameraList();

    template <imageColorMode M, imageBitDepth D>
    uEyeHandle<M, D> openCamera(const uEyeCameraInfo, std::function<void(int, std::string, std::chrono::time_point<std::chrono::system_clock>)> = nullptr, std::function<void(uEyeCameraInfo, std::chrono::milliseconds, progress_state &)> = uploadProgressHandlerBar);
}