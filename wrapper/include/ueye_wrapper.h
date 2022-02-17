/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 * 
 * Copyright (C) 2020, Arne Wendt
 * 
 * */


#pragma once


#include <tuple>
#include <vector>
#include <string>
#include <map>


#define CAMERA_AUTOCONF_IP "0.0.0.0/0"
#define CAMERA_SET_IP_MAX_RETRYS 3


#define CAMERA_AUTO_STARTER_FIRMWARE_UPLOAD 0x20
#define CAMERA_AUTO_STARTER_FIRMWARE_UPLOAD_PROGRESSBAR 0x40
#define CAMERA_AUTO_EXIT_HANDLE 0x80

#define CAMERA_STARTER_FIRMWARE_AUTO_UPLOAD_RETRYS 3

#define IMAGE_BUFFER_SIZE 3
#define ALLOC_MEM_TRYS 10
#define ACTIVATE_MEM_TRYS 10
#define FREE_MEM_TRYS 500
#define UNLOCK_MEM_TRYS 10
#define LOCK_MEM_TRYS 10

class uEyeWrapper
{
    private:
    static void setCameraIP(uEyeCam&, const DWORD&, const DWORD&, int = 0);
    static void _showProgress(int, bool*, bool*);


    public:
    uEyeWrapper();
    static cameraList getCameraList(LIST_OPTIONS = CAMERA_LIST_WITH_CONNECTION_INFO); // | CAMERA_LIST_WITH_SENSOR_INFO);
    static void getCameraConnectionInfo(uEyeCam&);
    static void getCameraConnectionInfo(cameraList&);
    static void setCameraIP(uEyeCam&, const std::string&);
    static void setCameraIPRangeStaticAuto(cameraList&, const std::string&, bool = false);
    static void setCameraIPAutoConf(uEyeCam&);
    static void setCameraIPAutoConf(cameraList&);
    static void setCameraAutoIPRange(uEyeCam&, const std::string&, const std::string&);
    static void setCameraAutoIPRange(cameraList&, const std::string&, const std::string&);

    static void openCamera(uEyeHandle&, uEyeCam&,
        IMAGE_OPTIONS = IMAGE_BGR_32_F,
        CAMERA_OPTIONS =    CAMERA_AUTO_STARTER_FIRMWARE_UPLOAD |
                            CAMERA_AUTO_STARTER_FIRMWARE_UPLOAD_PROGRESSBAR |
                            CAMERA_AUTO_OPTIMAL_CLK_MAX_FPS);
   
    void showErrorReport();

    ~uEyeWrapper();
};