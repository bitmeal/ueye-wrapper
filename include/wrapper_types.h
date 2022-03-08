#pragma once

#include <string>
#include <iostream>
#include <tuple>
#include <vector>

#include <stdint.h>

#include <ueye.h>

#ifdef _WIN32
#include <windows.h>
#else
// typedef uint32_t DWORD;
typedef unsigned long DWORD;
#endif

namespace uEyeWrapper
{
    enum class connectionType
    {
        USB,
        ETH
    };
    enum class sensorType
    {
        MONO,
        BGR
    };

    enum class colorMode
    {
        IMAGE_MONO_8_INT,
        IMAGE_MONO_32_F,
        IMAGE_BGR_8_INT,
        IMAGE_BGR_32_F
    };

    struct uEyeCameraInfo
    {
        DWORD cameraId;
        DWORD deviceId;

        std::string modelName;
        std::string serialNo;

        bool canOpen;

        connectionType connection;
        std::string IP;
        bool isIPautoconf;

        // std::tuple<int, int> sensorResolution;
        // sensorType sensor;
    };

    enum class whiteBalance : unsigned int
    {
        AUTO = 0, // gets special handling

        incandescent = 2800,
        halogen = 3200,
        fluorescent = 4000,
        dusk = 5000,
        dawn = 5000,
        sunlight = 5650, // 5500-5800
        flash = 6000,
        strobe = 6000,
        clouds = 7000,   // 6500-7500
        overcast = 7000, // 6500-7500
        fog = 8000
    };

    typedef std::vector</*const*/ uEyeCameraInfo> cameraList;

    // for use in firmware upload progress feedback helper
    enum class progress_state
    {
        running,
        failure,
        complete
    };
}