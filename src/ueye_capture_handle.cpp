#include "ueye_capture_handle.h"
using namespace std::chrono_literals;
#include <ctime>
#include <iomanip>
#include <cmath>

#include <fmt/core.h>
#include <fmt/ranges.h>
#include <fmt/color.h>
#include <fmt/chrono.h>

#include <plog/Log.h>

namespace uEyeWrapper
{
    // call api methods, log info, throw on error and perform cleanup
    // if message string is zero length, the API will be queried for last error string
    template <typename H, captureType C>
    UEYE_API_CALL_MEMBER_DEF(uEyeCaptureHandle<H, C>)
    {
        auto _msg = msg;
        const std::string common_prefix = fmt::format(
            "[{}@{}] capture handle {{camera {} ({} [#{}])}}",
            caller_name,
            caller_line,
            _camera_handle.camera.deviceId,
            _camera_handle.camera.modelName,
            _camera_handle.camera.serialNo);

        int nret = std::apply(f, f_args);
        if (nret != IS_SUCCESS)
        {
            // query API for error message if user supplied message is empty and return code is IS_NO_SUCCESS
            if (_msg.length() == 0 && nret == IS_NO_SUCCESS)
            {
                PLOG_DEBUG << fmt::format("{}: querying API for error message", common_prefix);
                auto err_info = _camera_handle._get_last_error_msg();
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

    template <typename H, captureType C>
    uEyeCaptureHandle<H, C>::uEyeCaptureHandle(const H &camera_handle, imageCallbackT imageCallback) : _camera_handle(camera_handle),
                                                                                                       imageCallback(imageCallback),
                                                                                                       _pool(camera_handle._concurrency)
    {
        _SPAWN_image_dispatcher();

        PLOG_INFO << fmt::format("capture handle {{camera {} ({} [#{}])}} image dispatcher running; using pool with {} threads for callback execution", // timestamp will be formated without milliseconds by default
                                 _camera_handle.camera.deviceId,
                                 _camera_handle.camera.modelName,
                                 _camera_handle.camera.serialNo,
                                 _pool.get_thread_count());

        _start_capture();
    }

    template <typename H, captureType C>
    template <typename enable_SFINAE>
    typename std::enable_if_t<C == captureType::TRIGGER, enable_SFINAE>
    uEyeCaptureHandle<H, C>::trigger(bool wait)
    // void uEyeCaptureHandle<H, C>::trigger()
    {
        /////////////////////////////////////////////////////////////
        // compile time method hiding test
        static_assert(
            C == captureType::TRIGGER,
            "uEyeCaptureHandle::trigger() method being compiled for captureType::LIVE - our SFINAE method hiding broke!");
        /////////////////////////////////////////////////////////////
        // impl

        UEYE_API_CALL(is_FreezeVideo, {_camera_handle.handle, wait ? IS_WAIT : IS_DONT_WAIT});
    }

    template <typename H, captureType C>
    void uEyeCaptureHandle<H, C>::_start_capture()
    {
        UEYE_API_CALL(is_SetExternalTrigger, {_camera_handle.handle, IS_SET_TRIGGER_SOFTWARE});

        switch (C)
        {
        case captureType::TRIGGER:
            break;
        case captureType::LIVE:
            UEYE_API_CALL(is_CaptureVideo, {_camera_handle.handle, IS_DONT_WAIT});
            break;
        default:
            throw std::logic_error("unknown capture mode"); // should only ever be evaluated if values modified by debugger!?
        }
    }

    template <typename H, captureType C>
    void uEyeCaptureHandle<H, C>::_SPAWN_image_dispatcher()
    {
        // check if already executing
        if (_image_dispatcher_executor.joinable())
        {
            return;
        }

        auto dispatcher = [&]()
        {
            PLOG_DEBUG << fmt::format("capture handle {{camera {} ({} [#{}])}} image dispatcher started",
                                      _camera_handle.camera.deviceId,
                                      _camera_handle.camera.modelName,
                                      _camera_handle.camera.serialNo);
            UINT events[] = {IS_SET_EVENT_FRAME, IS_SET_EVENT_TERMINATE_CAPTURE_THREADS};
            IS_WAIT_EVENTS wait_events = {events, sizeof(events) / sizeof(events[0]), FALSE, INFINITE, 0, 0};

            while (IS_SET_EVENT_TERMINATE_CAPTURE_THREADS != wait_events.nSignaled)
            {
                INT ret = is_Event(_camera_handle.handle, IS_EVENT_CMD_WAIT, &wait_events, sizeof(wait_events));
                PLOG_DEBUG << fmt::format("capture handle {{camera {} ({} [#{}])}} image dispatcher received event",
                                          _camera_handle.camera.deviceId,
                                          _camera_handle.camera.modelName,
                                          _camera_handle.camera.serialNo);

                if ((IS_SUCCESS == ret) && (IS_SET_EVENT_FRAME == wait_events.nSignaled))
                {
                    PLOG_DEBUG << fmt::format("capture handle {{camera {} ({} [#{}])}} image dispatcher GOT IMAGE/FRAME",
                                              _camera_handle.camera.deviceId,
                                              _camera_handle.camera.modelName,
                                              _camera_handle.camera.serialNo);
                    try
                    {
                        // get buffer for last captured image
                        INT _seqBuffNum;
                        char *_currMemPtr;
                        char *imgMemPtr;
                        UEYE_API_CALL(is_GetActSeqBuf, {_camera_handle.handle, &_seqBuffNum, &_currMemPtr, &imgMemPtr});

                        INT imgMemID = _camera_handle._memory_manager.getID(imgMemPtr);

                        PLOG_DEBUG << fmt::format("capture handle {{camera {} ({} [#{}])}} image in buffer {}[@{}]",
                                                  _camera_handle.camera.deviceId,
                                                  _camera_handle.camera.modelName,
                                                  _camera_handle.camera.serialNo,
                                                  imgMemID,
                                                  fmt::ptr(imgMemPtr));

                        // lock buffer
                        UEYE_API_CALL(is_LockSeqBuf, {_camera_handle.handle, IS_IGNORE_PARAMETER, imgMemPtr});

                        // query image info and build timestamp
                        UEYEIMAGEINFO imgInfo;
                        UEYE_API_CALL(is_GetImageInfo, {_camera_handle.handle, imgMemID, &imgInfo, (INT)sizeof(imgInfo)});

                        std::tm tt;
                        tt.tm_year = imgInfo.TimestampSystem.wYear - 1900;
                        tt.tm_mon = imgInfo.TimestampSystem.wMonth - 1;
                        tt.tm_mday = imgInfo.TimestampSystem.wDay;
                        tt.tm_hour = imgInfo.TimestampSystem.wHour;
                        tt.tm_min = imgInfo.TimestampSystem.wMinute;
                        tt.tm_sec = imgInfo.TimestampSystem.wSecond;
                        tt.tm_isdst = 0; // TODO: have to initialize member; query if is DST

                        auto c_tt = mktime(&tt);
                        auto millis = std::chrono::milliseconds(imgInfo.TimestampSystem.wMilliseconds);
                        auto timestamp = std::chrono::system_clock::from_time_t(c_tt) + millis;

                        PLOG_INFO << fmt::format("capture handle {{camera {} ({} [#{}])}} image #{}({}) @{}.{:03}", // timestamp will be formated without milliseconds by default
                                                 _camera_handle.camera.deviceId,
                                                 _camera_handle.camera.modelName,
                                                 _camera_handle.camera.serialNo,
                                                 imgInfo.u64TimestampDevice,
                                                 imgInfo.u64FrameNumber,
                                                 timestamp,
                                                 millis.count());

                        // callback executor task
                        auto caller = [=, this]()
                        {
                            try
                            {
                                auto imgView = typedImageViewT(
                                    (uint8_t *)imgMemPtr,
                                    {sln::PixelLength(std::get<0>(_camera_handle._resolution)),
                                     sln::PixelLength(std::get<1>(_camera_handle._resolution))});

                                if (_camera_handle._uEye_color_mode == IS_CM_RGB12_UNPACKED) // RGB 16bit is actually 12bit
                                {
                                    PLOG_DEBUG << fmt::format("capture handle {{camera {} ({} [#{}])}} image #{}({}) correcting 12bit <--> 16bit value scaling", // timestamp will be formated without milliseconds by default
                                                              _camera_handle.camera.deviceId,
                                                              _camera_handle.camera.modelName,
                                                              _camera_handle.camera.serialNo,
                                                              imgInfo.u64TimestampDevice,
                                                              imgInfo.u64FrameNumber);

                                    sln::for_each_pixel(imgView,
                                                        [](auto &px)
                                                        {
                                                            constexpr double scaler = 65536 / 4096; //(std::pow(2, 16) - 1) / (std::pow(2, 12) - 1);
                                                            px *= scaler;
                                                        });
                                }

                                // dispatch callback with a selene image view
                                imageCallback(
                                    // imgView.constant_view(),
                                    imgView.view(),
                                    timestamp,
                                    imgInfo.u64TimestampDevice,
                                    imgInfo.u64FrameNumber);
                            }
                            catch (const std::exception &e)
                            {
                                PLOG_ERROR << fmt::format("capture handle {{camera {} ({} [#{}])}} error while executing callback for image #{}({}): {}",
                                                          _camera_handle.camera.deviceId,
                                                          _camera_handle.camera.modelName,
                                                          _camera_handle.camera.serialNo,
                                                          imgInfo.u64TimestampDevice,
                                                          imgInfo.u64FrameNumber,
                                                          e.what());
                            }
                            // unlock buffer
                            UEYE_API_CALL(is_UnlockSeqBuf, {_camera_handle.handle, IS_IGNORE_PARAMETER, imgMemPtr});
                        };

                        // dispatch callback to threadpool
                        _pool.push_task(caller);
                    }
                    catch (...)
                    {
                        PLOG_ERROR << fmt::format("capture handle {{camera {} ({} [#{}])}} FAILED TO QUERY DRIVER FOR CURRENT BUFFER AND INFO",
                                                  _camera_handle.camera.deviceId,
                                                  _camera_handle.camera.modelName,
                                                  _camera_handle.camera.serialNo);
                    }
                }
            }
            PLOG_DEBUG << fmt::format("capture handle {{camera {} ({} [#{}])}} image dispatcher shut down",
                                      _camera_handle.camera.deviceId,
                                      _camera_handle.camera.modelName,
                                      _camera_handle.camera.serialNo);
        };

        _image_dispatcher_executor = std::thread(dispatcher);
    }

    template <typename H, captureType C>
    void uEyeCaptureHandle<H, C>::_stop_threads()
    {
        PLOG_DEBUG << fmt::format("capture handle {{camera {} ({} [#{}])}} sending termination signal to background threads",
                                  _camera_handle.camera.deviceId,
                                  _camera_handle.camera.modelName,
                                  _camera_handle.camera.serialNo);

        // send event signal IS_SET_EVENT_TERMINATE_CAPTURE_THREADS
        UINT terminate_event = IS_SET_EVENT_TERMINATE_CAPTURE_THREADS;
        while (is_Event(_camera_handle.handle, IS_EVENT_CMD_SET, &terminate_event, sizeof(terminate_event)) != IS_SUCCESS)
        {
            PLOG_WARNING << fmt::format("capture handle {{camera {} ({} [#{}])}} failed signaling threads to terminate, retrying...",
                                        _camera_handle.camera.deviceId,
                                        _camera_handle.camera.modelName,
                                        _camera_handle.camera.serialNo);
            std::this_thread::sleep_for(CAMERA_CLOSE_RETRY_WAIT);
        }

        // join image dispatcher observer
        if (_image_dispatcher_executor.joinable())
        {
            _image_dispatcher_executor.join();
        }

        // join pool
        PLOG_DEBUG << fmt::format("capture handle {{camera {} ({} [#{}])}} waiting for running image callbacks to finish",
                                  _camera_handle.camera.deviceId,
                                  _camera_handle.camera.modelName,
                                  _camera_handle.camera.serialNo);
        _pool.wait_for_tasks();

        // reset event signal
        PLOG_DEBUG << fmt::format("capture handle {{camera {} ({} [#{}])}} resetting termination signal to background threads",
                                  _camera_handle.camera.deviceId,
                                  _camera_handle.camera.modelName,
                                  _camera_handle.camera.serialNo);

        while (is_Event(_camera_handle.handle, IS_EVENT_CMD_RESET, &terminate_event, sizeof(terminate_event)) != IS_SUCCESS)
        {
            PLOG_WARNING << fmt::format("capture handle {{camera {} ({} [#{}])}} failed resetting terminate signal, retrying...",
                                        _camera_handle.camera.deviceId,
                                        _camera_handle.camera.modelName,
                                        _camera_handle.camera.serialNo);
            std::this_thread::sleep_for(CAMERA_CLOSE_RETRY_WAIT);
        }
    }

    template <typename H, captureType C>
    void uEyeCaptureHandle<H, C>::_stop_capture()
    {
        // TODO: error handling
        UEYE_API_CALL(is_ForceTrigger, {_camera_handle.handle}); // stop all running capturing operations
        UEYE_API_CALL(is_SetExternalTrigger, {_camera_handle.handle, IS_SET_TRIGGER_OFF});
        UEYE_API_CALL(is_SetExternalTrigger, {_camera_handle.handle, IS_GET_TRIGGER_STATUS}); // from @anqixu/ueye_cam: documentation seems to suggest that this is needed to disable external trigger mode (to go into free-run mode)
        UEYE_API_CALL(is_StopLiveVideo, {_camera_handle.handle, IS_WAIT});
    }

    template <typename H, captureType C>
    uEyeCaptureHandle<H, C>::~uEyeCaptureHandle()
    {
        _stop_capture();
        _stop_threads();
    }

    // explicitly instantiate templates
    template class uEyeCaptureHandle<uEyeHandle<uEye_MONO_8>, captureType::LIVE>;
    template class uEyeCaptureHandle<uEyeHandle<uEye_RGB_8>, captureType::LIVE>;
    template class uEyeCaptureHandle<uEyeHandle<uEye_MONO_16>, captureType::LIVE>;
    template class uEyeCaptureHandle<uEyeHandle<uEye_RGB_16>, captureType::LIVE>;
    template class uEyeCaptureHandle<uEyeHandle<uEye_MONO_8>, captureType::TRIGGER>;
    template class uEyeCaptureHandle<uEyeHandle<uEye_RGB_8>, captureType::TRIGGER>;
    template class uEyeCaptureHandle<uEyeHandle<uEye_MONO_16>, captureType::TRIGGER>;
    template class uEyeCaptureHandle<uEyeHandle<uEye_RGB_16>, captureType::TRIGGER>;

    template void uEyeCaptureHandle<uEyeHandle<uEye_MONO_8>, captureType::TRIGGER>::trigger<void>(bool);
    template void uEyeCaptureHandle<uEyeHandle<uEye_RGB_8>, captureType::TRIGGER>::trigger<void>(bool);
    template void uEyeCaptureHandle<uEyeHandle<uEye_MONO_16>, captureType::TRIGGER>::trigger<void>(bool);
    template void uEyeCaptureHandle<uEyeHandle<uEye_RGB_16>, captureType::TRIGGER>::trigger<void>(bool);
}