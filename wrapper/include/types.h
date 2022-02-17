#pragma once

#include <string>
#include <iostream>
#include <tuple>
#include <vector>

#include <stdint.h>

#ifdef _WIN32
	#include <windows.h>
#else
	// typedef uint32_t DWORD;
	typedef unsigned long DWORD;
#endif


namespace uEyeWrapper
{
    enum class connectionType {USB, ETH};
    enum class sensorType {MONO, BGR};

    enum class colorMode {
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

        bool inUse;

        connectionType connection;
        std::tuple<int, int> sensorResolution;
        sensorType colorMode;

        std::string IP;
        bool isIPautoconf;

        friend std::ostream& operator<<(std::ostream&, const uEyeCameraInfo&);
    };

    typedef std::vector<uEyeCameraInfo> cameraList;
}