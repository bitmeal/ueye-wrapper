#pragma once

#include "ueye_handle.h"

#include <type_traits>
#include <math.h>
#include <functional>
#include <thread>
using namespace std::chrono_literals;

#include <deque>

#include <stdio.h>

// get isatty function; use STDOUTTTY as truthy "value"
#ifdef __linux__
#include <unistd.h>
#define STDOUTTTY isatty(fileno(stdout))
#elif _WIN32
#include <io.h>
#define STDOUTTTY _isatty(_fileno(stdout))
#else
#error could not detect istty method
#endif

#include <fmt/core.h>
#include <fmt/ranges.h>
#include <fmt/color.h>

#include <plog/Log.h>

// #define NOMINMAX <-- defined globally on library target in CMake!
#include <indicators/cursor_control.hpp>
#include <indicators/progress_bar.hpp>
#include <indicators/terminal_size.hpp>

#define CAMERA_STARTER_FIRMWARE_UPLOAD_RETRY_WAIT 10ms
#define CAMERA_STARTER_FIRMWARE_UPLOAD_RETRIES 3
#define CAMERA_CLOSE_RETRY_WAIT 10ms
#define CAMERA_CLOSE_RETRIES 3

// TODO: after open make logging print camera handle only
namespace uEyeWrapper
{
    void uploadProgressHandlerBar(uEyeCameraInfo camera, std::chrono::milliseconds duration, progress_state &state)
    {
        // only show bar if is stdout is connected to a tty
        if (STDOUTTTY)
        {
            // init progress bar
            const auto update_interval = 250ms;
            const double update_slice = 100.0f * update_interval / duration;

            const std::string prefix = fmt::format("Uploading FW: {} ({}[#{}]) ", camera.deviceId, camera.modelName, camera.serialNo);
            const size_t reserved = std::char_traits<char>::length(prefix.c_str()) + std::char_traits<char>::length("[] XXX% ");
            // const size_t bar_width = indicators::terminal_width() - reserved;

            indicators::ProgressBar bar{
                indicators::option::BarWidth(indicators::terminal_width() - reserved),
                indicators::option::PrefixText{prefix},
                indicators::option::ShowPercentage{true}};

            // advance bar in fixed interval
            auto t_start = std::chrono::high_resolution_clock::now();
            for (size_t progress = 0; progress * update_slice < 100; progress++)
            {
                // update bar width
                bar.set_option(indicators::option::BarWidth(indicators::terminal_width() - reserved));

                // handle state change
                if (state == progress_state::failure)
                {
                    bar.mark_as_completed();
                    return;
                }
                if (state == progress_state::complete)
                {
                    break;
                }

                // advance progress bar and wait for next tick
                bar.set_progress((size_t)(progress * update_slice));
                // std::this_thread::sleep_for(update_interval);
                std::this_thread::sleep_until(t_start + (progress + 1) * update_interval);
            }
            bar.set_progress(100);
            // bar.mark_as_completed();
        }
        else
        {
            PLOG_DEBUG << fmt::format(
                "camera {} ({} [#{}]) no tty on stdout; hiding firmware upload progress bar",
                camera.deviceId,
                camera.modelName,
                camera.serialNo);
        }
    }

    /////////////////////////////////////////////////
    // uEyeHandle impl

    template <colorMode T>
    uEyeHandle<T>::uEyeHandle(
        uEyeCameraInfo camera,
        std::function<void(int, std::string)> errorCallback,
        std::function<void(uEyeCameraInfo, std::chrono::milliseconds, progress_state &)> uploadProgressHandler) : /*FPS(_FPS), freerun_active(_freerun_active),*/ camera(_camera), resolution(_resolution), sensor(_sensor), errorCallback(errorCallback), handle(0)
    {
        // initialize object
        _camera = camera; // param
        _camera.canOpen = false;

        // open camera
        // exceptions will not be caught; _open_camera will cleanup after itself on failure
        _open_camera(uploadProgressHandler);

        // get remaining info and set default configurations (auto everything)
        // catch and rethrow exceptions after cleanup
        try
        {
            _populate_sensor_info();
            
            _set_AutoControl_default();
            setWhiteBalance(whiteBalance::AUTO);
        }
        catch (...)
        {
            // cleanup
            _close_camera();

            // rethrow
            throw;
        }
    };

    template <colorMode T>
    void uEyeHandle<T>::_open_camera(std::function<void(uEyeCameraInfo, std::chrono::milliseconds, progress_state &)> uploadProgressHandler)
    {
        PLOG_INFO << fmt::format(
            "opening camera {}: {} [#{}]",
            camera.deviceId,
            camera.modelName,
            camera.serialNo);

        // open camera
        HIDS hCam;

        int uploadRetrys = 0;
        while (++uploadRetrys <= CAMERA_STARTER_FIRMWARE_UPLOAD_RETRIES)
        {
            // try opening camera
            hCam = camera.cameraId;
            INT nret = is_InitCamera(&hCam, NULL);

            if (nret != IS_SUCCESS)
            {
                PLOG_INFO << fmt::format(
                    "opening camera {}: {} [#{}] failed; checking for starter firmware...",
                    camera.deviceId,
                    camera.modelName,
                    camera.serialNo);
                // requires new starter firmware?
                if (nret == IS_STARTER_FW_UPLOAD_NEEDED)
                {
                    PLOG_INFO << fmt::format(
                        "camera {} ({} [#{}]) requires new starter firmware",
                        camera.deviceId,
                        camera.modelName,
                        camera.serialNo);
                    // query approx firmware upload time
                    int msUpdate;
                    if (is_GetDuration(hCam, IS_SE_STARTER_FW_UPLOAD, &msUpdate) != IS_SUCCESS)
                        throw std::runtime_error("could not request info for starter firmware upload");

                    // add 1s/1000ms to estimate
                    msUpdate += 1000;

                    PLOG_DEBUG << fmt::format(
                        "camera {} ({} [#{}]) firmware upload time ~{}ms",
                        camera.deviceId,
                        camera.modelName,
                        camera.serialNo,
                        msUpdate);

                    // make valid progress handler function if none present
                    if (!uploadProgressHandler)
                    {
                        uploadProgressHandler = [=](uEyeCameraInfo, std::chrono::milliseconds, progress_state &)
                        {
                            PLOG_DEBUG << fmt::format(
                                "camera {} ({} [#{}]) using empty progress handler",
                                camera.deviceId,
                                camera.modelName,
                                camera.serialNo);
                        };
                    }

                    // init
                    auto state = progress_state::running;

                    // spawn progress handler
                    std::thread progress_handler_executor;
                    progress_handler_executor = std::thread(
                        uploadProgressHandler,
                        camera,
                        std::chrono::milliseconds(msUpdate),
                        std::ref(state));

                    // open and upload firmware
                    PLOG_INFO << fmt::format(
                        "camera {} ({} [#{}]) starting firmware upload...",
                        camera.deviceId,
                        camera.modelName,
                        camera.serialNo);
                    hCam = hCam | IS_ALLOW_STARTER_FW_UPLOAD;
                    nret = is_InitCamera(&hCam, NULL);

                    // set state for progress handler
                    state = nret == IS_SUCCESS ? progress_state::complete : progress_state::failure;

                    // wait for progress handler
                    progress_handler_executor.join();

                    if (nret == IS_SUCCESS)
                    {
                        // camera is open
                        break;
                    }

                    // prepare retry if error code suggests a possible success
                    if (nret == IS_CANT_OPEN_DEVICE || nret == IS_DEVICE_ALREADY_PAIRED)
                    {
                        // assign handle and try closing camera
                        handle = hCam;
                        _close_camera();
                    }

                    PLOG_INFO << fmt::format(
                        "failed to open camera {} ({} [#{}]) after starter firmware upload retry {}/{}",
                        camera.deviceId,
                        camera.modelName,
                        camera.serialNo,
                        uploadRetrys,
                        CAMERA_STARTER_FIRMWARE_UPLOAD_RETRIES);

                    std::this_thread::sleep_for(CAMERA_STARTER_FIRMWARE_UPLOAD_RETRY_WAIT);
                }
                else
                {
                    PLOG_INFO << fmt::format(
                        "failed to open camera {} ({} [#{}]) is_InitCamera() returned with code {}; raising exception!",
                        camera.deviceId,
                        camera.modelName,
                        camera.serialNo,
                        nret);
                    throw std::runtime_error(fmt::format("opening camera with device id {} failed; is_InitCamera() returned with code {}", camera.deviceId, nret));
                }
            }
            else // camera is open
            {
                PLOG_INFO << fmt::format(
                    "camera {} ({} [#{}]) opened and initialized successfully",
                    camera.deviceId,
                    camera.modelName,
                    camera.serialNo);
                break;
            }
        }

        if (uploadRetrys > CAMERA_STARTER_FIRMWARE_UPLOAD_RETRIES)
        {
            PLOG_INFO << fmt::format(
                "failed to open camera {} ({} [#{}]) after starter firmware upload retry limit; raising exception!",
                camera.deviceId,
                camera.modelName,
                camera.serialNo);
            throw std::runtime_error("opening camera failed; too many firmware upload retries");
        }

        // opened successfully; assign handle
        handle = hCam;
    }

    template <colorMode T>
    void uEyeHandle<T>::_populate_sensor_info()
    {
        SENSORINFO sensorInfo;
        if (is_GetSensorInfo(handle, &sensorInfo) != IS_SUCCESS)
        {
            PLOG_INFO << fmt::format(
                "camera {} ({} [#{}]) failed to query sensor information; raising exception",
                camera.deviceId,
                camera.modelName,
                camera.serialNo);

            throw std::runtime_error("failed to query sensor information");
        }

        _resolution = {sensorInfo.nMaxWidth, sensorInfo.nMaxHeight};
        switch (sensorInfo.nColorMode)
        {
        case IS_COLORMODE_MONOCHROME:
            _sensor = sensorType::MONO;
            break;
        case IS_COLORMODE_BAYER:
            _sensor = sensorType::BGR;
            break;
        }

        PLOG_INFO << fmt::format(
            "camera {} ({} [#{}]) sensor: {} @{}x{}px",
            camera.deviceId,
            camera.modelName,
            camera.serialNo,
            _sensor == sensorType::MONO ? "monochrome" : "color",
            std::get<0>(_resolution),
            std::get<1>(_resolution));
    }

    template <colorMode T>
    void uEyeHandle<T>::_set_AutoControl_default()
    {
        // query default config and apply it

        // setup configuration structures
        UINT nSizeOfParam = sizeof(AES_CONFIGURATION) - sizeof(CHAR) + sizeof(AES_PEAK_CONFIGURATION);
        CHAR *pBuffer = new char[nSizeOfParam];
        memset(pBuffer, 0, nSizeOfParam);

        AES_CONFIGURATION *pAesConfiguration = (AES_CONFIGURATION *)pBuffer;
        pAesConfiguration->nMode = IS_AES_MODE_PEAK;
        AES_PEAK_CONFIGURATION *pPeakConfiguration = (AES_PEAK_CONFIGURATION *)pAesConfiguration->pConfiguration;

        INT nEnable = IS_AUTOPARAMETER_ENABLE;

        // query and set config
        PLOG_DEBUG << fmt::format(
            "camera {} ({} [#{}]) enabling auto control, querying and setting default parameters",
            camera.deviceId,
            camera.modelName,
            camera.serialNo);

        // || operator uses short circuit evaluation: second half will not be evaluated if first does not succeed
        if (
            is_AutoParameter(handle, IS_AES_CMD_GET_CONFIGURATION_DEFAULT, pAesConfiguration, nSizeOfParam) != IS_SUCCESS ||
            is_AutoParameter(handle, IS_AES_CMD_SET_ENABLE, &nEnable, sizeof(nEnable)) != IS_SUCCESS ||
            is_AutoParameter(handle, IS_AES_CMD_SET_CONFIGURATION, pAesConfiguration, nSizeOfParam) != IS_SUCCESS)
        {
            // failed
            PLOG_INFO << fmt::format(
                "camera {} ({} [#{}]) enabling auto control with default parameters failed; raising exception",
                camera.deviceId,
                camera.modelName,
                camera.serialNo);

            // cleanup
            delete pBuffer;

            throw std::runtime_error("enabling auto control on camera failed");
        }
        else
        {
            // success
            // TODO: print values acquired from camera
            PLOG_INFO << fmt::format(
                "camera {} ({} [#{}]) auto control enabled",
                camera.deviceId,
                camera.modelName,
                camera.serialNo);
        }

        // cleanup
        delete pBuffer;
    }

    template <colorMode T>
    void uEyeHandle<T>::setWhiteBalance(whiteBalance WB)
    {
        if (WB == whiteBalance::AUTO)
        {
            _set_WhiteBalance_AUTO(true);
        }
        else
        {
            _set_WhiteBalance_kelvin(static_cast<std::underlying_type<whiteBalance>::type>(WB));
        }
    }

    template <colorMode T>
    void uEyeHandle<T>::setWhiteBalance(int kelvin)
    {
        _set_WhiteBalance_kelvin(kelvin);
    }

    template <colorMode T>
    unsigned int uEyeHandle<T>::_set_WhiteBalance_colorModel_default()
    {
        PLOG_DEBUG << fmt::format(
            "camera {} ({} [#{}]) setting white balance color model to default",
            camera.deviceId,
            camera.modelName,
            camera.serialNo);

        // set default color model
        UINT colorModel;
        if (
            is_ColorTemperature(handle, COLOR_TEMPERATURE_CMD_GET_RGB_COLOR_MODEL_DEFAULT, &colorModel, sizeof(colorModel)) != IS_SUCCESS ||
            is_ColorTemperature(handle, COLOR_TEMPERATURE_CMD_SET_RGB_COLOR_MODEL, &colorModel, sizeof(colorModel)) != IS_SUCCESS)
        {
            // failed
            PLOG_INFO << fmt::format(
                "camera {} ({} [#{}]) setting default color model for white balance failed; raising exception",
                camera.deviceId,
                camera.modelName,
                camera.serialNo);

            throw std::runtime_error("setting white balance default color model failed");
        }

        PLOG_INFO << fmt::format(
            "camera {} ({} [#{}]) set default white balance color model ({})",
            camera.deviceId,
            camera.modelName,
            camera.serialNo,
            colorModel);

        return colorModel;
    }

    template <colorMode T>
    void uEyeHandle<T>::_set_WhiteBalance_kelvin(unsigned int kelvin)
    {
        PLOG_DEBUG << fmt::format(
            "camera {} ({} [#{}]) setting white balance; disabling auto white balance",
            camera.deviceId,
            camera.modelName,
            camera.serialNo);

        _set_WhiteBalance_AUTO(false);

        try
        {
            _set_WhiteBalance_colorModel_default();
        }
        catch (...)
        {
            PLOG_WARNING << fmt::format(
                "camera {} ({} [#{}]) setting default color model for white balance failed; using current (unknown)",
                camera.deviceId,
                camera.modelName,
                camera.serialNo);
        }

        // query color temperature range
        UINT tempMin;
        UINT tempMax;
        if (
            is_ColorTemperature(handle, COLOR_TEMPERATURE_CMD_GET_TEMPERATURE_MIN, &tempMin, sizeof(tempMin)) != IS_SUCCESS ||
            is_ColorTemperature(handle, COLOR_TEMPERATURE_CMD_GET_TEMPERATURE_MAX, &tempMax, sizeof(tempMax)) != IS_SUCCESS)
        {
            // failed
            PLOG_WARNING << fmt::format(
                "camera {} ({} [#{}]) querying white balance color temperature range failed; trying to set without range check",
                camera.deviceId,
                camera.modelName,
                camera.serialNo);

            // throw std::runtime_error("setting white balance default color model failed");
        }
        else
        {
            PLOG_INFO << fmt::format(
                "camera {} ({} [#{}]) white balance color temperature range [{}K-{}K]",
                camera.deviceId,
                camera.modelName,
                camera.serialNo,
                tempMin,
                tempMax);

            // adjust temperature to range
            kelvin = std::max(tempMin, std::min(tempMax, kelvin));
        }

        PLOG_INFO << fmt::format(
            "camera {} ({} [#{}]) setting white balance color temperature as {}K",
            camera.deviceId,
            camera.modelName,
            camera.serialNo,
            kelvin);

        INT kelvin_typed = kelvin;
        if (is_ColorTemperature(handle, COLOR_TEMPERATURE_CMD_SET_TEMPERATURE, &kelvin_typed, sizeof(kelvin_typed)) != IS_SUCCESS)
        {
            PLOG_INFO << fmt::format(
                "camera {} ({} [#{}]) setting white balance color temperature failed; raising exception",
                camera.deviceId,
                camera.modelName,
                camera.serialNo);

            throw std::runtime_error("setting white balance color temperature failed");
        }

        PLOG_INFO << fmt::format(
            "camera {} ({} [#{}]) set white balance color temperature as {}K - OK",
            camera.deviceId,
            camera.modelName,
            camera.serialNo,
            kelvin);

        // TODO: reread and "verify"/return actual color temperature
    }

    template <colorMode T>
    void uEyeHandle<T>::_set_WhiteBalance_AUTO(bool enable)
    {
        UINT nEnable = enable ? IS_AUTOPARAMETER_ENABLE : IS_AUTOPARAMETER_DISABLE;

        // prepare enabling
        if (enable)
        {
            // ENABLE AUTO WB

            // query supported auto white balance types
            UINT nSupportedTypes = 0;
            UINT WBMode = 0;

            if (is_AutoParameter(handle, IS_AWB_CMD_GET_SUPPORTED_TYPES, (void *)&nSupportedTypes, sizeof(nSupportedTypes)) != IS_SUCCESS)
            {
                PLOG_INFO << fmt::format(
                    "camera {} ({} [#{}]) failed querying supported auto white balance modes; raising exception",
                    camera.deviceId,
                    camera.modelName,
                    camera.serialNo);

                throw std::runtime_error("failed querying supported auto white balance modes");
            }

            // select white balance mode: temperature preceeding grey world
            if (nSupportedTypes & IS_AWB_COLOR_TEMPERATURE)
            {
                WBMode = IS_AWB_COLOR_TEMPERATURE;
                PLOG_INFO << fmt::format(
                    "camera {} ({} [#{}]) using auto white balance mode 'color temperature'",
                    camera.deviceId,
                    camera.modelName,
                    camera.serialNo);
            }
            else if (nSupportedTypes & IS_AWB_GREYWORLD)
            {
                WBMode = IS_AWB_GREYWORLD;
                PLOG_INFO << fmt::format(
                    "camera {} ({} [#{}]) using auto white balance mode 'grey world'",
                    camera.deviceId,
                    camera.modelName,
                    camera.serialNo);
            }
            else
            {
                PLOG_INFO << fmt::format(
                    "camera {} ({} [#{}]) no known auto white balance modes supported; raising exception",
                    camera.deviceId,
                    camera.modelName,
                    camera.serialNo);

                throw std::runtime_error("no known auto white balance modes supported");
            }

            // set auto white balance type

            // set auto white balance color model
            // query (and set) default WB color model
            UINT defaultColorModel = _set_WhiteBalance_colorModel_default();
            INT nSupportedModels = WBMode;

            if (
                is_AutoParameter(handle, IS_AWB_CMD_GET_SUPPORTED_RGB_COLOR_MODELS, (void *)&nSupportedModels, sizeof(nSupportedModels)) != IS_SUCCESS ||
                !nSupportedModels & defaultColorModel ||
                is_AutoParameter(handle, IS_AWB_CMD_SET_RGB_COLOR_MODEL, (void *)&defaultColorModel, sizeof(defaultColorModel)) != IS_SUCCESS)
            {
                PLOG_WARNING << fmt::format(
                    "camera {} ({} [#{}]) failed to set default white balance color model as auto white balance color model; using current (unknown)",
                    camera.deviceId,
                    camera.modelName,
                    camera.serialNo);
            }

            PLOG_INFO << fmt::format(
                "camera {} ({} [#{}]) set auto white balance to use color model {}",
                camera.deviceId,
                camera.modelName,
                camera.serialNo,
                defaultColorModel);

            // set to enable auto WB
            nEnable = IS_AUTOPARAMETER_ENABLE;
        }

        // enable/disable
        if (is_AutoParameter(handle, IS_AWB_CMD_SET_ENABLE, (void *)&nEnable, sizeof(nEnable)) != IS_SUCCESS)
        {
            PLOG_INFO << fmt::format(
                "camera {} ({} [#{}]) failed to {} auto white balance; raising exception",
                camera.deviceId,
                camera.modelName,
                camera.serialNo,
                nEnable == IS_AUTOPARAMETER_ENABLE ? "enable" : "disable");

            throw std::runtime_error(fmt::format("failed to {} auto white balance", nEnable == IS_AUTOPARAMETER_ENABLE ? "enable" : "disable"));
        }
    }

    template <colorMode T>
    double uEyeHandle<T>::setFPS(double FPS)
    {
        PLOG_INFO << fmt::format(
            "camera {} ({} [#{}]) requested setting FPS to {}",
            camera.deviceId,
            camera.modelName,
            camera.serialNo,
            FPS);

        double frameTimingMin, frameTimingMax, frameTimingIntervall;
        double minFPS, maxFPS;
        UINT clk;
        std::vector<UINT> clkList;

        // get timing/FPS range
        if (is_GetFrameTimeRange(handle, &frameTimingMin, &frameTimingMax, &frameTimingIntervall) != IS_SUCCESS)
        {
            PLOG_INFO << fmt::format(
                "camera {} ({} [#{}]) failed to query frame timing (FPS) range; raising exception",
                camera.deviceId,
                camera.modelName,
                camera.serialNo);

            throw std::runtime_error("failed to query frame timing (FPS) range");
        }
        minFPS = 1 / frameTimingMax;
        maxFPS = 1 / frameTimingMin;

        PLOG_INFO << fmt::format(
            "camera {} ({} [#{}]) FPS range for current pixel clock [{}-{}]",
            camera.deviceId,
            camera.modelName,
            camera.serialNo,
            minFPS,
            maxFPS);

        // FPS out of current pixel clocks range
        if (maxFPS < FPS)
        {
            PLOG_INFO << fmt::format(
                "camera {} ({} [#{}]) requested FPS out of current pixel clock range; adjusting pixel clock...",
                camera.deviceId,
                camera.modelName,
                camera.serialNo);

            // get pixel clock
            if (is_PixelClock(handle, IS_PIXELCLOCK_CMD_GET, (void *)&clk, sizeof(clk)) != IS_SUCCESS)
            {
                PLOG_INFO << fmt::format(
                    "camera {} ({} [#{}]) failed to query current pixel clock; raising exception",
                    camera.deviceId,
                    camera.modelName,
                    camera.serialNo);

                throw std::runtime_error("failed to query current pixel clock");
            }

            PLOG_INFO << fmt::format(
                "camera {} ({} [#{}]) current pixel clock {}MHz",
                camera.deviceId,
                camera.modelName,
                camera.serialNo,
                clk);

            // get possible clocks

            // METHOD 1
            UINT clkRange[3];
            std::memset(clkRange, 0, sizeof(clkRange));
            if ((is_PixelClock(handle, IS_PIXELCLOCK_CMD_GET_RANGE, (void *)clkRange, sizeof(clkRange)) == IS_SUCCESS) && (clkRange[2] != 0))
            {
                PLOG_INFO << fmt::format(
                    "camera {} ({} [#{}]) pixel clock range [{}-{}]MHz, skip {}MHz",
                    camera.deviceId,
                    camera.modelName,
                    camera.serialNo,
                    clkRange[0], clkRange[1], clkRange[2]);

                // build vector of possible clock values from range and skip
                clkList.reserve((clkRange[1] - clkRange[0]) / clkRange[2] + 1);
                for (int n = 0; clkRange[0] + n * clkRange[2] <= clkRange[1]; ++n)
                    clkList.push_back(clkRange[0] + n * clkRange[2]);
            }
            // METHOD 2
            else // got no info, as increment was 0 -> only a few discrete clocks available
            {
                UINT numClk = 0;
                if (is_PixelClock(handle, IS_PIXELCLOCK_CMD_GET_NUMBER, (void *)&numClk, sizeof(UINT)) != IS_SUCCESS || numClk == 0)
                {
                    PLOG_INFO << fmt::format(
                        "camera {} ({} [#{}]) failed to query number of discrete pixel clock values; raising exception",
                        camera.deviceId,
                        camera.modelName,
                        camera.serialNo);

                    throw std::runtime_error("failed to query number of discrete pixel clock values");
                }

                PLOG_DEBUG << fmt::format(
                    "camera {} ({} [#{}]) using {} discrete pixel clock values; querying values...",
                    camera.deviceId,
                    camera.modelName,
                    camera.serialNo,
                    numClk);

                clkList = std::vector<UINT>(numClk, 0);
                if (is_PixelClock(handle, IS_PIXELCLOCK_CMD_GET_LIST, (void *)clkList.data(), numClk * sizeof(UINT)) != IS_SUCCESS)
                {
                    PLOG_INFO << fmt::format(
                        "camera {} ({} [#{}]) failed to query discrete pixel clock values; raising exception",
                        camera.deviceId,
                        camera.modelName,
                        camera.serialNo);

                    throw std::runtime_error("failed to query discrete pixel clock values");
                }
            }

            PLOG_INFO << fmt::format(
                "camera {} ({} [#{}]) trying to find lowest pixel clock capable of supporting desired FPS from {}",
                camera.deviceId,
                camera.modelName,
                camera.serialNo,
                clkList);

            // vector has available clocks set; pop elements from front til we reach the first element greater than our current clock
            auto nextClk = std::find_if(clkList.begin(), clkList.end(), [clk](UINT clkElem)
                                        { return clkElem > clk; });
            std::deque<UINT> clkOpts(nextClk, clkList.end());

            // loop through pixel clocks till we can set desired FPS
            while (FPS > maxFPS && !clkOpts.empty()) //desired FPS within range
            {
                // set clock
                PLOG_INFO << fmt::format(
                    "camera {} ({} [#{}]) setting pixel clock to {}MHz",
                    camera.deviceId,
                    camera.modelName,
                    camera.serialNo,
                    clkOpts.front());

                if (is_PixelClock(handle, IS_PIXELCLOCK_CMD_SET, (void *)&(clkOpts.front()), sizeof(UINT)) != IS_SUCCESS)
                {
                    PLOG_INFO << fmt::format(
                        "camera {} ({} [#{}]) failed setting pixel clock to {}MHz; raising exception",
                        camera.deviceId,
                        camera.modelName,
                        camera.serialNo,
                        clkOpts.front());

                    throw std::runtime_error(fmt::format("failed setting pixel clock to {}MHz", clkOpts.front()));
                }

                // get FPS range
                if (is_GetFrameTimeRange(handle, &frameTimingMin, &frameTimingMax, &frameTimingIntervall) != IS_SUCCESS)
                {
                    PLOG_INFO << fmt::format(
                        "camera {} ({} [#{}]) failed to query frame timing (FPS) range; raising exception",
                        camera.deviceId,
                        camera.modelName,
                        camera.serialNo);

                    throw std::runtime_error("failed to query frame timing (FPS) range");
                }
                minFPS = 1 / frameTimingMax;
                maxFPS = 1 / frameTimingMin;

                PLOG_INFO << fmt::format(
                    "camera {} ({} [#{}]) FPS range for current pixel clock(@{}MHz) [{}-{}]",
                    camera.deviceId,
                    camera.modelName,
                    camera.serialNo,
                    clkOpts.front(),
                    minFPS,
                    maxFPS);

                clkOpts.pop_front();
            }
        }

        //set PFS after (when necessary) adjusting pixel clock
        double newFPS;
        if (is_SetFrameRate(handle, FPS, &(newFPS)) != IS_SUCCESS)
        {
            PLOG_INFO << fmt::format(
                "camera {} ({} [#{}]) failed to set FPS; raising exception",
                camera.deviceId,
                camera.modelName,
                camera.serialNo);

            throw std::runtime_error("failed to set FPS");
        }

        PLOG_INFO << fmt::format(
            "camera {} ({} [#{}]) requested {} FPS; actual {}",
            camera.deviceId,
            camera.modelName,
            camera.serialNo,
            FPS,
            newFPS);

        return newFPS;
    }

    template <colorMode T>
    uEyeHandle<T>::~uEyeHandle()
    {
        _close_camera();
        _cleanup_memory();
    }

    template <colorMode T>
    void uEyeHandle<T>::_close_camera()
    {
        PLOG_INFO << fmt::format("closing connection to camera {}({}) [{:#010x}]", _camera.deviceId, _camera.cameraId, handle);
        if (handle != 0)
        {
            int tries = 0;
            while (tries < CAMERA_CLOSE_RETRIES) // has 3 tries
            {
                if (is_ExitCamera(handle) == IS_SUCCESS)
                    break;

                tries++;
                std::this_thread::sleep_for(CAMERA_CLOSE_RETRY_WAIT);
            }
            if (tries == CAMERA_CLOSE_RETRIES)
            {
                PLOG_WARNING << fmt::format("failed closing connection to camera {}({}) [{:#010x}] after {} tries!", _camera.deviceId, _camera.cameraId, handle, tries);
            }
            else
            {
                PLOG_INFO << fmt::format("closed connection to camera {}({}) [{:#010x}]", _camera.deviceId, _camera.cameraId, handle);
            }
        }
        else
        {
            PLOG_INFO << fmt::format("never opened connection to camera {}({}) [{:#010x}]", _camera.deviceId, _camera.cameraId, handle);
        }
    }

    template <colorMode T>
    void uEyeHandle<T>::_cleanup_memory()
    {
    }

    // instantiate templates
    template class uEyeHandle<colorMode::IMAGE_MONO_8_INT>;
    template class uEyeHandle<colorMode::IMAGE_MONO_32_F>;
    template class uEyeHandle<colorMode::IMAGE_BGR_8_INT>;
    template class uEyeHandle<colorMode::IMAGE_BGR_32_F>;

    // // acquisition handle
    // uEyeImageAcquisitionHandle::uEyeImageAcquisitionHandle(HIDS handle, std::function<void(T)> imgCallback, std::function<void(int, std::string)> errorInfoCallback)
    // {

    // }

    // void uEyeImageAcquisitionHandle::startVideo(int FPS)
    // {

    // }

    // void uEyeImageAcquisitionHandle::stopVideo()
    // {

    // }

    // void uEyeImageAcquisitionHandle::trigger()
    // {

    // }

    // void uEyeHandle::setExtTrigger();
}