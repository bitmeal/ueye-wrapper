#pragma once

#include <string>
#include <iostream>
#include <tuple>
#include <vector>
#include <chrono>
#include <map>
#include <functional>
#include <algorithm>

#include <stdint.h>

#include <ueye.h>

#ifdef _WIN32
#include <windows.h>
#else
// typedef uint32_t DWORD;
// typedef unsigned long DWORD;
#endif


// image type template parameter helpers
#define uEye_MONO_8 uEyeWrapper::imageColorMode::MONO, uEyeWrapper::imageBitDepth::i8
#define uEye_RGB_8 uEyeWrapper::imageColorMode::RGB, uEyeWrapper::imageBitDepth::i8
#define uEye_MONO_16 uEyeWrapper::imageColorMode::MONO, uEyeWrapper::imageBitDepth::i16
#define uEye_RGB_16 uEyeWrapper::imageColorMode::RGB, uEyeWrapper::imageBitDepth::i16

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
        RGB
    };

    enum class imageColorMode : size_t
    {
        MONO = 1,
        RGB = 3
    };

    // TODO: add and correctly handle 12 bit depth?
    enum class imageBitDepth : size_t
    {
        i8 = 8,
        i16 = 16
    };

    enum class captureType
    {
        TRIGGER,
        LIVE
    };

    // enum class colorMode
    // {
    //     MONO_8,
    //     MONO_16,
    //     RGB_8,
    //     RGB_16
    // };

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

    struct captureError
    {
        captureError(std::string name, std::string info) : name(name), info(info){};

        std::string name;
        std::string info;
        std::vector<std::chrono::time_point<std::chrono::system_clock>> timestamps;

        size_t count() { return timestamps.size(); };
        void add(std::chrono::time_point<std::chrono::system_clock> timestamp = std::chrono::system_clock::now()) { timestamps.push_back(timestamp); };
    };

    struct captureErrors
    {
        captureError API_NO_DEST_MEM;
        captureError API_CONVERSION_FAILED;
        captureError API_IMAGE_LOCKED;
        captureError DRV_OUT_OF_BUFFERS;
        captureError DRV_DEVICE_NOT_READY;
        captureError TRANSFER_FAILED;
        captureError &USB_TRANSFER_FAILED;
        captureError DEV_MISSED_IMAGES;
        captureError DEV_TIMEOUT;
        captureError DEV_FRAME_CAPTURE_FAILED;
        captureError ETH_BUFFER_OVERRUN;
        captureError &ETH_MISSED_IMAGES;

        const std::map<const UEYE_CAPTURE_STATUS, captureError &> errorMapper;

        captureErrors() : API_NO_DEST_MEM("API_NO_DEST_MEM", "Not enough destination memory allocated or all destination buffers locked by the application"),
                          API_CONVERSION_FAILED("API_CONVERSION_FAILED", "The current image could not be processed correctly (Internal error during internal processing of the image)"),
                          API_IMAGE_LOCKED("API_IMAGE_LOCKED", "All destination buffers locked by the application"),
                          DRV_OUT_OF_BUFFERS("DRV_OUT_OF_BUFFERS", "No free internal image memory is available to the driver"),
                          DRV_DEVICE_NOT_READY("DRV_DEVICE_NOT_READY", "The camera is no longer available"),
                          TRANSFER_FAILED("TRANSFER_FAILED", "Not enough free bandwidth for transferring the image"),
                          DEV_MISSED_IMAGES("DEV_MISSED_IMAGES", "The camera's frame rate is too high or the bandwidth on the network is insufficient to transfer the image"),
                          DEV_TIMEOUT("DEV_TIMEOUT", "The maximum allowable time for image capturing in the camera was exceeded"),
                          DEV_FRAME_CAPTURE_FAILED("DEV_FRAME_CAPTURE_FAILED", "Not enough free bandwidth on the interface for transferring the image"),
                          ETH_BUFFER_OVERRUN("ETH_BUFFER_OVERRUN", "The sensor transfers more data than the internal camera memory of the GigE uEye camera can accommodate (The selected data rate of the sensor is too high)"),
                          USB_TRANSFER_FAILED(TRANSFER_FAILED),
                          ETH_MISSED_IMAGES(DEV_MISSED_IMAGES),
                          errorMapper({{IS_CAP_STATUS_API_NO_DEST_MEM, API_NO_DEST_MEM},
                                       {IS_CAP_STATUS_API_CONVERSION_FAILED, API_CONVERSION_FAILED},
                                       {IS_CAP_STATUS_API_IMAGE_LOCKED, API_IMAGE_LOCKED},
                                       {IS_CAP_STATUS_DRV_OUT_OF_BUFFERS, DRV_OUT_OF_BUFFERS},
                                       {IS_CAP_STATUS_DRV_DEVICE_NOT_READY, DRV_DEVICE_NOT_READY},
                                       {IS_CAP_STATUS_TRANSFER_FAILED, TRANSFER_FAILED},
                                       {IS_CAP_STATUS_USB_TRANSFER_FAILED, USB_TRANSFER_FAILED},
                                       {IS_CAP_STATUS_DEV_MISSED_IMAGES, DEV_MISSED_IMAGES},
                                       {IS_CAP_STATUS_DEV_TIMEOUT, DEV_TIMEOUT},
                                       {IS_CAP_STATUS_DEV_FRAME_CAPTURE_FAILED, DEV_FRAME_CAPTURE_FAILED},
                                       {IS_CAP_STATUS_ETH_BUFFER_OVERRUN, ETH_BUFFER_OVERRUN},
                                       {IS_CAP_STATUS_ETH_MISSED_IMAGES, ETH_MISSED_IMAGES}}){};

        void update(UEYE_CAPTURE_STATUS_INFO status, std::chrono::time_point<std::chrono::system_clock> timestamp, std::function<void(const captureError)> onErrorCallback)
        {
            std::for_each(errorMapper.begin(), errorMapper.end(), [&](auto &errorMapping)
                          {
                              if (status.adwCapStatusCnt_Detail[errorMapping.first])
                              {
                                  errorMapping.second.add(timestamp);
                                  if (onErrorCallback)
                                  {
                                      onErrorCallback(errorMapping.second);
                                  }
                              }
                          });
        };
    };

    // for use in firmware upload progress feedback helper
    enum class progress_state
    {
        running,
        failure,
        complete
    };
}
