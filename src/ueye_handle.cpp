#include "ueye_handle.h"
#include "ueye_wrapper.h"

#include <type_traits>
#include <math.h>
#include <deque>
#include <algorithm>
using namespace std::chrono_literals;

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
#include <fmt/chrono.h>

#include <plog/Log.h>

// #define NOMINMAX <-- defined globally on library target in CMake!
#include <indicators/cursor_control.hpp>
#include <indicators/progress_bar.hpp>
#include <indicators/terminal_size.hpp>

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
            auto t_start = std::chrono::steady_clock::now();
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
            PLOG_WARNING << fmt::format(
                "camera {} ({} [#{}]) requires a new starter firmware; upload time ~{}; upload in progress...",
                camera.deviceId,
                camera.modelName,
                camera.serialNo,
                duration);
        }
    }

    /////////////////////////////////////////////////
    // uEyeHandle impl

    // call api methods, log info, throw on error and perform cleanup
    // if message string is zero length, the API will be queried for last error string
    template <imageColorMode M, imageBitDepth D>
    UEYE_API_CALL_MEMBER_DEF(uEyeHandle<M, D>)
    {
        auto _msg = msg;
        const std::string common_prefix = fmt::format(
            "[{}@{}] camera {} ({} [#{}])",
            caller_name,
            caller_line,
            camera.deviceId,
            camera.modelName,
            camera.serialNo);

        int nret = std::apply(f, f_args);
        if (nret != IS_SUCCESS)
        {
            // query API for error message if user supplied message is empty and return code is IS_NO_SUCCESS
            if (_msg.length() == 0 && nret == IS_NO_SUCCESS)
            {
                PLOG_DEBUG << fmt::format("{}: querying API for error message", common_prefix);
                auto err_info = _get_last_error_msg();
                _msg = std::get<1>(err_info);
            }

            // build common message
            const std::string common_msg = fmt::format(
                "{}; {}() returned with code {}",
                _msg.length() == 0 ? "<empty>" : _msg,
                f_name,
                nret);

            // log the error as warning from wrapper; error handling shall be done by user
            PLOG_WARNING << fmt::format("{}: {}", common_prefix, common_msg);

            // if a cleanup is required and a handler function is provided, execute it
            if (cleanup_handler)
            {
                PLOG_WARNING << fmt::format("{}: calling provided cleanup handler after failed call to {}()", common_prefix, f_name);
                cleanup_handler();
            }

            // throw error
            throw std::runtime_error(common_msg);
        }

        // log API method name and return code for debugging purposes (nret will allways be IS_SUCCESS(0) here)
        PLOG_DEBUG << fmt::format("{}: {}() returned with code {}", common_prefix, f_name, nret);
    }

    template <imageColorMode M, imageBitDepth D>
    uEyeHandle<M, D>::uEyeHandle(
        uEyeCameraInfo camera,
        std::function<void(int, std::string, std::chrono::time_point<std::chrono::system_clock>)> captureStatusCallback,
        std::function<void(uEyeCameraInfo, std::chrono::milliseconds, progress_state &)> uploadProgressHandler) : /*FPS(_FPS), freerun_active(_freerun_active),*/
                                                                                                                  camera(_camera),
                                                                                                                  resolution(_resolution),
                                                                                                                  sensor(_sensor),
                                                                                                                  errorStats(_error_stats),
                                                                                                                  captureStatusCallback(captureStatusCallback),
                                                                                                                  handle(0),
                                                                                                                  _channels((std::underlying_type_t<decltype(M)>)M),
                                                                                                                  _bit_depth((std::underlying_type_t<decltype(D)>)D),
                                                                                                                  _uEye_color_mode(M == imageColorMode::MONO ?                                                                     // switch on color channels
                                                                                                                                       (D == imageBitDepth::i8 ? IS_CM_MONO8 : IS_CM_MONO16)                                       // mono
                                                                                                                                                             : (D == imageBitDepth::i8 ? IS_CM_RGB8_PACKED : IS_CM_RGB12_UNPACKED) // RGB
                                                                                                                                   ),
                                                                                                                  _concurrency(uEyeWrapper::concurrency),
                                                                                                                  _memory_manager(*this),
                                                                                                                  _events_init({{IS_SET_EVENT_FRAME, FALSE, FALSE},
                                                                                                                                // start capture status event with signal flag and force initial handler execution
                                                                                                                                {IS_SET_EVENT_CAPTURE_STATUS, FALSE, TRUE},
                                                                                                                                // terminate thread event will not reset and will be available continuously after signaling
                                                                                                                                {IS_SET_EVENT_TERMINATE_HANDLE_THREADS, TRUE, FALSE},
                                                                                                                                {IS_SET_EVENT_TERMINATE_CAPTURE_THREADS, TRUE, FALSE}})
    {
        // setup data
        std::transform(_events_init.begin(), _events_init.end(),
                       std::back_inserter(_events),
                       [](IS_INIT_EVENT &e)
                       {
                           return e.nEvent;
                       });

        // initialize object
        _camera = camera; // param
        _camera.canOpen = false;

        PLOG_INFO << fmt::format(
            "camera {}: {} [#{}] to be opened with {} color channels @{}bit (IS_CM_* == {}); allowed conurrency {}",
            camera.deviceId,
            camera.modelName,
            camera.serialNo,
            _channels,
            _bit_depth,
            _uEye_color_mode,
            _concurrency);

        // open camera
        // exceptions will not be caught; _open_camera will cleanup after itself on failure
        _open_camera(uploadProgressHandler);

        // get remaining info and set default configurations (auto everything)
        // catch and rethrow exceptions after cleanup
        try
        {
            _populate_sensor_info();

            _setup_capture_to_memory();

            _init_events();
            _SPAWN_capture_status_observer();

            _set_AutoControl_default();
            setWhiteBalance(whiteBalance::AUTO);
        }
        catch (...)
        {
            // cleanup
            _memory_manager.cleanup();
            _stop_threads();
            _close_camera();

            // rethrow
            throw;
        }
    };

    template <imageColorMode M, imageBitDepth D>
    template <captureType C>
    uEyeCaptureHandle<uEyeHandle<M, D>, C> uEyeHandle<M, D>::getCaptureHandle(typename uEyeCaptureHandle<uEyeHandle<M, D>, C>::imageCallbackT imageCallback)
    {
        return uEyeCaptureHandle<uEyeHandle<M, D>, C>(*this, imageCallback);
    }

    template <imageColorMode M, imageBitDepth D>
    void uEyeHandle<M, D>::_open_camera(std::function<void(uEyeCameraInfo, std::chrono::milliseconds, progress_state &)> uploadProgressHandler)
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

        UEYE_API_CALL(is_ResetToDefault, {handle});
    }

    template <imageColorMode M, imageBitDepth D>
    void uEyeHandle<M, D>::_populate_sensor_info()
    {
        SENSORINFO sensorInfo;

        UEYE_API_CALL(is_GetSensorInfo, {handle, &sensorInfo});

        _resolution = {sensorInfo.nMaxWidth, sensorInfo.nMaxHeight};
        switch (sensorInfo.nColorMode)
        {
        case IS_COLORMODE_MONOCHROME:
            _sensor = sensorType::MONO;
            break;
        case IS_COLORMODE_BAYER:
            _sensor = sensorType::RGB;
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

    template <imageColorMode M, imageBitDepth D>
    void uEyeHandle<M, D>::_set_AutoControl_default()
    {
        // TODO: make configurable
        // enable HDR; has to be enabled before white balance and auto exposure settings
        try
        {
            INT nHDR;
            UEYE_API_CALL(is_GetHdrMode, {handle, &nHDR});
            if (nHDR != IS_HDR_NOT_SUPPORTED)
            {
                PLOG_DEBUG << fmt::format(
                    "camera {} ({} [#{}]) supports HDR: enabling",
                    camera.deviceId,
                    camera.modelName,
                    camera.serialNo);

                INT enable = IS_ENABLE_HDR;
                UEYE_API_CALL(is_EnableHdr, {handle, enable});
            }
        }
        catch (...)
        {
        }

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

        UEYE_API_CALL(is_AutoParameter, {handle, IS_AES_CMD_GET_CONFIGURATION_DEFAULT, pAesConfiguration, nSizeOfParam}, [&]()
                      { delete pBuffer; });
        UEYE_API_CALL(is_AutoParameter, {handle, IS_AES_CMD_SET_ENABLE, &nEnable, (UINT)sizeof(nEnable)}, [&]()
                      { delete pBuffer; });
        UEYE_API_CALL(is_AutoParameter, {handle, IS_AES_CMD_SET_CONFIGURATION, pAesConfiguration, nSizeOfParam}, [&]()
                      { delete pBuffer; });

        // TODO: print values acquired from camera
        PLOG_INFO << fmt::format(
            "camera {} ({} [#{}]) auto control enabled",
            camera.deviceId,
            camera.modelName,
            camera.serialNo);

        // cleanup
        delete pBuffer;
    }

    template <imageColorMode M, imageBitDepth D>
    void uEyeHandle<M, D>::setWhiteBalance(whiteBalance WB)
    {
        if (WB == whiteBalance::AUTO)
        {
            _set_WhiteBalance_AUTO(true);
        }
        else
        {
            _set_WhiteBalance_kelvin(static_cast<std::underlying_type_t<whiteBalance>>(WB));
        }
    }

    template <imageColorMode M, imageBitDepth D>
    void uEyeHandle<M, D>::setWhiteBalance(int kelvin)
    {
        _set_WhiteBalance_kelvin(kelvin);
    }

    template <imageColorMode M, imageBitDepth D>
    unsigned int uEyeHandle<M, D>::_set_WhiteBalance_colorModel_default()
    {
        PLOG_DEBUG << fmt::format(
            "camera {} ({} [#{}]) setting white balance color model to default",
            camera.deviceId,
            camera.modelName,
            camera.serialNo);

        // set default color model
        UINT colorModel;
        UEYE_API_CALL(is_ColorTemperature, {handle, COLOR_TEMPERATURE_CMD_GET_RGB_COLOR_MODEL_DEFAULT, &colorModel, (UINT)sizeof(colorModel)});
        UEYE_API_CALL(is_ColorTemperature, {handle, COLOR_TEMPERATURE_CMD_SET_RGB_COLOR_MODEL, &colorModel, (UINT)sizeof(colorModel)});

        PLOG_INFO << fmt::format(
            "camera {} ({} [#{}]) set default white balance color model ({})",
            camera.deviceId,
            camera.modelName,
            camera.serialNo,
            colorModel);

        return colorModel;
    }

    template <imageColorMode M, imageBitDepth D>
    void uEyeHandle<M, D>::_set_WhiteBalance_kelvin(unsigned int kelvin)
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
        UEYE_API_CALL(is_ColorTemperature, {handle, COLOR_TEMPERATURE_CMD_SET_TEMPERATURE, &kelvin_typed, (UINT)sizeof(kelvin_typed)});

        PLOG_INFO << fmt::format(
            "camera {} ({} [#{}]) set white balance color temperature as {}K - OK",
            camera.deviceId,
            camera.modelName,
            camera.serialNo,
            kelvin);

        // TODO: reread and "verify"/return actual color temperature
    }

    template <imageColorMode M, imageBitDepth D>
    void uEyeHandle<M, D>::_set_WhiteBalance_AUTO(bool enable)
    {
        UINT nEnable = enable && M == imageColorMode::RGB ? IS_AUTOPARAMETER_ENABLE : IS_AUTOPARAMETER_DISABLE;

        // prepare enabling
        if (enable)
        {
            // ENABLE AUTO WB

            // query supported auto white balance types
            UINT nSupportedTypes = 0;
            UINT WBMode = 0;
            UEYE_API_CALL(is_AutoParameter, {handle, IS_AWB_CMD_GET_SUPPORTED_TYPES, (void *)&nSupportedTypes, (UINT)sizeof(nSupportedTypes)});

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
            UEYE_API_CALL(is_AutoParameter, {handle, IS_AWB_CMD_SET_TYPE, (void *)&WBMode, (UINT)sizeof(WBMode)});

            // set auto white balance color model if using IS_AWB_COLOR_TEMPERATURE
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
        }

        // enable/disable
        UEYE_API_CALL(is_AutoParameter, {handle, IS_AWB_CMD_SET_ENABLE, (void *)&nEnable, (UINT)sizeof(nEnable)});
        // , fmt::format("failed to {} auto white balance", nEnable == IS_AUTOPARAMETER_ENABLE ? "enable" : "disable"));
    }

    template <imageColorMode M, imageBitDepth D>
    double uEyeHandle<M, D>::setFPS(double FPS)
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
        UEYE_API_CALL(is_GetFrameTimeRange, {handle, &frameTimingMin, &frameTimingMax, &frameTimingIntervall});
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
            UEYE_API_CALL(is_PixelClock, {handle, IS_PIXELCLOCK_CMD_GET, (void *)&clk, (UINT)sizeof(clk)});

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
                UEYE_API_CALL(is_PixelClock, {handle, IS_PIXELCLOCK_CMD_GET_LIST, (void *)clkList.data(), (UINT)(numClk * sizeof(UINT))});
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

                UEYE_API_CALL(is_PixelClock, {handle, IS_PIXELCLOCK_CMD_SET, (void *)&(clkOpts.front()), (UINT)sizeof(UINT)});
                // , fmt::format("failed setting pixel clock to {}MHz", clkOpts.front()));

                // get FPS range
                UEYE_API_CALL(is_GetFrameTimeRange, {handle, &frameTimingMin, &frameTimingMax, &frameTimingIntervall});
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
        UEYE_API_CALL(is_SetFrameRate, {handle, FPS, &newFPS});

        PLOG_INFO << fmt::format(
            "camera {} ({} [#{}]) requested {} FPS; actual {}",
            camera.deviceId,
            camera.modelName,
            camera.serialNo,
            FPS,
            newFPS);

        return newFPS;
    }

    template <imageColorMode M, imageBitDepth D>
    std::tuple<int, std::string> uEyeHandle<M, D>::_get_last_error_msg() const
    {
        char *lastErrorBuffer;
        int lastError;

        if (is_GetError(handle, &lastError, &lastErrorBuffer) == IS_SUCCESS)
        {
            return {lastError, std::string(lastErrorBuffer)};
        }
        else
        {
            return {0, ""};
        }
    }

    template <imageColorMode M, imageBitDepth D>
    uEyeHandle<M, D>::~uEyeHandle()
    {
        _stop_threads();
        _cleanup_events();

        _memory_manager.cleanup(); // should be destructed and cleanup itself; debugging shows otherwise
        _close_camera();
        // _cleanup_memory();
    }

    template <imageColorMode M, imageBitDepth D>
    void uEyeHandle<M, D>::_init_events()
    {
        // TODO: use stl containers, make INIT_EVENT structure a member, build event list from member by mapping function
        // init events
        // IS_INIT_EVENT init_events[] = {
        //     {IS_SET_EVENT_FRAME, FALSE, FALSE},
        //     // start capture status event with signal flag and force initial handler execution
        //     {IS_SET_EVENT_CAPTURE_STATUS, FALSE, TRUE},
        //     // terminate thread event will not reset and will be available continuously after signaling
        //     {IS_SET_EVENT_TERMINATE_HANDLE_THREADS, TRUE, FALSE},
        //     {IS_SET_EVENT_TERMINATE_CAPTURE_THREADS, TRUE, FALSE}};
        // UEYE_API_CALL(is_Event, {handle, IS_EVENT_CMD_INIT, init_events, (UINT)sizeof(init_events)});

        UEYE_API_CALL(is_Event, {handle, IS_EVENT_CMD_INIT, _events_init.data(), (UINT)(sizeof(typename decltype(_events_init)::value_type) * _events_init.size())});

        // enable event messages
        // UINT events[] = {IS_SET_EVENT_FRAME, IS_SET_EVENT_CAPTURE_STATUS, IS_SET_EVENT_TERMINATE_HANDLE_THREADS, IS_SET_EVENT_TERMINATE_CAPTURE_THREADS};
        // is_Event(handle, IS_EVENT_CMD_ENABLE, events, sizeof(events));
        UEYE_API_CALL(is_Event, {handle, IS_EVENT_CMD_ENABLE, _events.data(), (UINT)(sizeof(typename decltype(_events)::value_type) * _events.size())});
    }

    template <imageColorMode M, imageBitDepth D>
    void uEyeHandle<M, D>::_cleanup_events()
    {
        // // TODO: as in _init_events()
        // UINT events[] = {IS_SET_EVENT_FRAME, IS_SET_EVENT_CAPTURE_STATUS, IS_SET_EVENT_TERMINATE_HANDLE_THREADS, IS_SET_EVENT_TERMINATE_CAPTURE_THREADS};
        // is_Event(handle, IS_EVENT_CMD_DISABLE, events, sizeof(events));
        // is_Event(handle, IS_EVENT_CMD_EXIT, events, sizeof(events));
        is_Event(handle, IS_EVENT_CMD_DISABLE, _events.data(), (UINT)(sizeof(typename decltype(_events)::value_type) * _events.size()));
        is_Event(handle, IS_EVENT_CMD_EXIT, _events.data(), (UINT)(sizeof(typename decltype(_events)::value_type) * _events.size()));
    }

    template <imageColorMode M, imageBitDepth D>
    void uEyeHandle<M, D>::_SPAWN_capture_status_observer()
    {
        // check if already executing
        if (_capture_status_observer_executor.joinable())
        {
            return;
        }

        // observer function:
        auto observer = [&]()
        {
            PLOG_DEBUG << fmt::format("camera {} ({} [#{}]) capture status observer started", camera.deviceId, camera.modelName, camera.serialNo);
            UINT events[] = {IS_SET_EVENT_CAPTURE_STATUS, IS_SET_EVENT_TERMINATE_HANDLE_THREADS};
            IS_WAIT_EVENTS wait_events = {events, sizeof(events) / sizeof(events[0]), FALSE, INFINITE, 0, 0};

            while (IS_SET_EVENT_TERMINATE_HANDLE_THREADS != wait_events.nSignaled)
            {
                INT ret = is_Event(handle, IS_EVENT_CMD_WAIT, &wait_events, sizeof(wait_events));
                PLOG_DEBUG << fmt::format("camera {} ({} [#{}]) capture status observer received event", camera.deviceId, camera.modelName, camera.serialNo);

                if ((IS_SUCCESS == ret) && (IS_SET_EVENT_CAPTURE_STATUS == wait_events.nSignaled))
                {
                    try
                    {
                        UEYE_CAPTURE_STATUS_INFO CaptureStatusInfo;
                        UEYE_API_CALL(is_CaptureStatus, {handle, IS_CAPTURE_STATUS_INFO_CMD_GET, (void *)&CaptureStatusInfo, (UINT)sizeof(CaptureStatusInfo)});

                        _error_stats.update(CaptureStatusInfo, std::chrono::system_clock::now(),
                                            [&this](captureError err)
                                            {
                                                PLOG_WARNING << fmt::format(
                                                    "camera {} ({} [#{}]) {}({}): {}",
                                                    camera.deviceId,
                                                    camera.modelName,
                                                    camera.serialNo,
                                                    err.name,
                                                    err.count(),
                                                    err.info);
                                            });
                    }
                    catch (const std::exception &e)
                    {
                        PLOG_ERROR << e.what();
                    }
                }
            }
            PLOG_DEBUG << fmt::format("camera {} ({} [#{}]) capture status observer shut down", camera.deviceId, camera.modelName, camera.serialNo);
        };

        _capture_status_observer_executor = std::thread(observer);
    }

    template <imageColorMode M, imageBitDepth D>
    void uEyeHandle<M, D>::_stop_threads()
    {
        PLOG_DEBUG << fmt::format("camera {} ({} [#{}]) sending termination signal to background threads", camera.deviceId, camera.modelName, camera.serialNo);

        // send event signal IS_SET_EVENT_TERMINATE_HANDLE_THREADS
        UINT terminate_event = IS_SET_EVENT_TERMINATE_HANDLE_THREADS;
        while (is_Event(handle, IS_EVENT_CMD_SET, &terminate_event, sizeof(terminate_event)) != IS_SUCCESS)
        {
            PLOG_WARNING << fmt::format(
                "camera {} ({} [#{}]) failed signaling threads to terminate, retrying...",
                camera.deviceId,
                camera.modelName,
                camera.serialNo);
            std::this_thread::sleep_for(CAMERA_CLOSE_RETRY_WAIT);
        }

        // join capture status observer
        if (_capture_status_observer_executor.joinable())
        {
            _capture_status_observer_executor.join();
        }
    }

    template <imageColorMode M, imageBitDepth D>
    void uEyeHandle<M, D>::_close_camera()
    {
        PLOG_INFO << fmt::format("closing connection to camera {}({}) [{:#010x}]", _camera.deviceId, _camera.cameraId, handle);
        if (handle != 0)
        {
            int tries = 0;
            while (tries < CAMERA_CLOSE_RETRIES) // has 3 tries
            {
                if (is_ExitCamera(handle) == IS_SUCCESS)
                    break;

                PLOG_WARNING << fmt::format("failed closing connection to camera {}({}) [{:#010x}] will retry", _camera.deviceId, _camera.cameraId, handle);
                tries++;
                std::this_thread::sleep_for(CAMERA_CLOSE_RETRY_WAIT);
            }
            if (tries == CAMERA_CLOSE_RETRIES)
            {
                PLOG_ERROR << fmt::format("failed closing connection to camera {}({}) [{:#010x}] after {} tries!", _camera.deviceId, _camera.cameraId, handle, tries);
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

    template <imageColorMode M, imageBitDepth D>
    void uEyeHandle<M, D>::_setup_capture_to_memory()
    {
        // set color mode
        UEYE_API_CALL(is_SetColorMode, {handle, _uEye_color_mode});
        // // fails to execut when forwarding to API method using std::apply from wrapper
        // INT nret = is_SetColorMode(handle, _uEye_color_mode);
        // if(nret != IS_SUCCESS)
        // {
        //     throw std::runtime_error("set color mode failed");
        // }

        // allocate and activate memory
        _memory_manager.initialize();

        // set capture to memory
        UEYE_API_CALL(is_SetDisplayMode, {handle, IS_SET_DM_DIB});
    }

    // will be handled by destruction of memory manager
    // template <imageColorMode M, imageBitDepth D>
    // void uEyeHandle<M, D>::_cleanup_memory()
    // {
    //     // all buffer/memory chunks have to be unlocked by now; they should as all threads should have been waited on
    // }

    // explicitly instantiate templates
    template class uEyeHandle<uEye_MONO_8>;
    template class uEyeHandle<uEye_RGB_8>;
    template class uEyeHandle<uEye_MONO_16>;
    template class uEyeHandle<uEye_RGB_16>;

    template uEyeCaptureHandle<uEyeHandle<uEye_MONO_8>, captureType::LIVE> uEyeHandle<uEye_MONO_8>::getCaptureHandle<captureType::LIVE>(typename uEyeCaptureHandle<uEyeHandle<uEye_MONO_8>, captureType::LIVE>::imageCallbackT);
    template uEyeCaptureHandle<uEyeHandle<uEye_RGB_8>, captureType::LIVE> uEyeHandle<uEye_RGB_8>::getCaptureHandle<captureType::LIVE>(typename uEyeCaptureHandle<uEyeHandle<uEye_RGB_8>, captureType::LIVE>::imageCallbackT);
    template uEyeCaptureHandle<uEyeHandle<uEye_MONO_16>, captureType::LIVE> uEyeHandle<uEye_MONO_16>::getCaptureHandle<captureType::LIVE>(typename uEyeCaptureHandle<uEyeHandle<uEye_MONO_16>, captureType::LIVE>::imageCallbackT);
    template uEyeCaptureHandle<uEyeHandle<uEye_RGB_16>, captureType::LIVE> uEyeHandle<uEye_RGB_16>::getCaptureHandle<captureType::LIVE>(typename uEyeCaptureHandle<uEyeHandle<uEye_RGB_16>, captureType::LIVE>::imageCallbackT);
    template uEyeCaptureHandle<uEyeHandle<uEye_MONO_8>, captureType::TRIGGER> uEyeHandle<uEye_MONO_8>::getCaptureHandle<captureType::TRIGGER>(typename uEyeCaptureHandle<uEyeHandle<uEye_MONO_8>, captureType::TRIGGER>::imageCallbackT);
    template uEyeCaptureHandle<uEyeHandle<uEye_RGB_8>, captureType::TRIGGER> uEyeHandle<uEye_RGB_8>::getCaptureHandle<captureType::TRIGGER>(typename uEyeCaptureHandle<uEyeHandle<uEye_RGB_8>, captureType::TRIGGER>::imageCallbackT);
    template uEyeCaptureHandle<uEyeHandle<uEye_MONO_16>, captureType::TRIGGER> uEyeHandle<uEye_MONO_16>::getCaptureHandle<captureType::TRIGGER>(typename uEyeCaptureHandle<uEyeHandle<uEye_MONO_16>, captureType::TRIGGER>::imageCallbackT);
    template uEyeCaptureHandle<uEyeHandle<uEye_RGB_16>, captureType::TRIGGER> uEyeHandle<uEye_RGB_16>::getCaptureHandle<captureType::TRIGGER>(typename uEyeCaptureHandle<uEyeHandle<uEye_RGB_16>, captureType::TRIGGER>::imageCallbackT);

    // call api methods, log info, throw on error and perform cleanup
    // if message string is zero length, the API will be queried for last error string
    template <typename H>
    UEYE_API_CALL_MEMBER_DEF(imageMemoryManager<H>)
    {
        auto _msg = msg;
        const std::string common_prefix = fmt::format(
            "[{}@{}] memory manager {{camera {} ({} [#{}])}}",
            caller_name,
            caller_line,
            _consumer_handle.camera.deviceId,
            _consumer_handle.camera.modelName,
            _consumer_handle.camera.serialNo);

        int nret = std::apply(f, f_args);
        if (nret != IS_SUCCESS)
        {
            // query API for error message if user supplied message is empty and return code is IS_NO_SUCCESS
            if (_msg.length() == 0 && nret == IS_NO_SUCCESS)
            {
                PLOG_DEBUG << fmt::format("{}: querying API for error message", common_prefix);
                auto err_info = _consumer_handle._get_last_error_msg();
                _msg = std::get<1>(err_info);
            }

            // build common message
            const std::string common_msg = fmt::format(
                "{}; {}() returned with code {}",
                _msg.length() == 0 ? "<empty>" : _msg,
                f_name,
                nret);

            // log the error as warning from wrapper; error handling shall be done by user
            PLOG_WARNING << fmt::format("{}: {}", common_prefix, common_msg);

            // if a cleanup is required and a handler function is provided, execute it
            if (cleanup_handler)
            {
                PLOG_WARNING << fmt::format("{}: calling provided cleanup handler after failed call to {}()", common_prefix, f_name);
                cleanup_handler();
            }

            // throw error
            throw std::runtime_error(common_msg);
        }

        // log API method name and return code for debugging purposes (nret will allways be IS_SUCCESS(0) here)
        PLOG_DEBUG << fmt::format("{}: {}() returned with code {}", common_prefix, f_name, nret);
    }

    template <typename H>
    imageMemoryManager<H>::imageMemoryManager(const H &consumer_handle) : _consumer_handle(consumer_handle) {}

    template <typename H>
    void imageMemoryManager<H>::initialize()
    {
        // allocate memory chunks equal to concurrency value
        auto [width, height] = _consumer_handle._resolution;
        auto bits_per_pixel = _consumer_handle._channels * _consumer_handle._bit_depth;

        PLOG_INFO << fmt::format(
            "memory manager {{camera {} ({} [#{}])}} allocating {} image buffers for {}x{}px@{}bit",
            _consumer_handle.camera.deviceId,
            _consumer_handle.camera.modelName,
            _consumer_handle.camera.serialNo,
            _consumer_handle._concurrency,
            width,
            height,
            bits_per_pixel);

        for (auto _ : times(_consumer_handle._concurrency))
        {
            INT memID = 0;
            char *memPtr = nullptr;

            try
            {
                UEYE_API_CALL(is_AllocImageMem, {_consumer_handle.handle, (INT)width, (INT)height, (INT)bits_per_pixel, &memPtr, &memID});
                PLOG_INFO << fmt::format(
                    "memory manager {{camera {} ({} [#{}])}} allocated image buffer {}[@{}]",
                    _consumer_handle.camera.deviceId,
                    _consumer_handle.camera.modelName,
                    _consumer_handle.camera.serialNo,
                    (int)memID,
                    fmt::ptr(memPtr));

                UEYE_API_CALL(is_AddToSequence, {_consumer_handle.handle, memPtr, memID});
                PLOG_INFO << fmt::format(
                    "memory manager {{camera {} ({} [#{}])}} activated image buffer {}[@{}]",
                    _consumer_handle.camera.deviceId,
                    _consumer_handle.camera.modelName,
                    _consumer_handle.camera.serialNo,
                    (int)memID,
                    fmt::ptr(memPtr));
            }
            catch (...)
            {
                PLOG_WARNING << fmt::format(
                    "memory manager {{camera {} ({} [#{}])}} failed to allocate and activate last image buffer!",
                    _consumer_handle.camera.deviceId,
                    _consumer_handle.camera.modelName,
                    _consumer_handle.camera.serialNo);

                // remove
                if (memPtr)
                {
                    UEYE_API_CALL(is_FreeImageMem, {_consumer_handle.handle, memPtr, memID});
                }
            }

            // store buffer in memory map for deactivation and deallocation
            (*this)[memPtr] = memID;
        }

        PLOG_INFO << fmt::format(
            "memory manager {{camera {} ({} [#{}])}} allocated {} image buffers",
            _consumer_handle.camera.deviceId,
            _consumer_handle.camera.modelName,
            _consumer_handle.camera.serialNo,
            size());

        if (!size())
        {
            PLOG_ERROR << fmt::format(
                "memory manager {{camera {} ({} [#{}])}} failed to allocate image buffers",
                _consumer_handle.camera.deviceId,
                _consumer_handle.camera.modelName,
                _consumer_handle.camera.serialNo);

            throw std::runtime_error("failed to allocate image buffers");
        }
    }

    template <typename H>
    void imageMemoryManager<H>::cleanup()
    {
        if (size())
        {
            PLOG_INFO << fmt::format(
                "memory manager {{camera {} ({} [#{}])}} will deactivate and deallocate {} image buffers",
                _consumer_handle.camera.deviceId,
                _consumer_handle.camera.modelName,
                _consumer_handle.camera.serialNo,
                size());

            try
            {
                UEYE_API_CALL(is_ClearSequence, {_consumer_handle.handle});
            }
            catch (...)
            {
            }

            // deallocate memory from driver
            for (auto it = this->begin(); it != this->end();)
            {
                auto [memPtr, memID] = *it;
                try
                {
                    UEYE_API_CALL(is_FreeImageMem, {_consumer_handle.handle, memPtr, memID});

                    PLOG_INFO << fmt::format(
                        "memory manager {{camera {} ({} [#{}])}} deallocated image buffer {}[@{}]",
                        _consumer_handle.camera.deviceId,
                        _consumer_handle.camera.modelName,
                        _consumer_handle.camera.serialNo,
                        (int)memID,
                        fmt::ptr(memPtr));
                }
                catch (...)
                {
                    PLOG_WARNING << fmt::format(
                        "memory manager {{camera {} ({} [#{}])}} failed to deallocate image buffer {}[@{}]",
                        _consumer_handle.camera.deviceId,
                        _consumer_handle.camera.modelName,
                        _consumer_handle.camera.serialNo,
                        (int)memID,
                        fmt::ptr(memPtr));
                }

                // remove freed from managed memories
                it = erase(it);
            }
        }
    }

    template <typename H>
    INT imageMemoryManager<H>::getID(char *bufferAddress) const
    {
        return at(bufferAddress);
    }

    template <typename H>
    imageMemoryManager<H>::~imageMemoryManager()
    {
        cleanup();
    }

    // explicitly instantiate templates
    template class imageMemoryManager<uEyeHandle<uEye_MONO_8>>;
    template class imageMemoryManager<uEyeHandle<uEye_RGB_8>>;
    template class imageMemoryManager<uEyeHandle<uEye_MONO_16>>;
    template class imageMemoryManager<uEyeHandle<uEye_RGB_16>>;

}