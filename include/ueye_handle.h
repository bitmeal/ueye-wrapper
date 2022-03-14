#pragma once

#include "wrapper_helpers.h"
#include "wrapper_types.h"
namespace uEyeWrapper
{
    template <imageColorMode M, imageBitDepth D>
    class uEyeHandle;
}

#include "ueye_capture_handle.h"

#include <selene/img/pixel/PixelTypeAliases.hpp>
#include <selene/img/dynamic/DynImageView.hpp>
#include <selene/img/typed/ImageView.hpp>
// #include <selene/img/typed/ImageViewAliases.hpp>

#include <functional>
#include <chrono>
#include <thread>
#include <map>

#define CAMERA_STARTER_FIRMWARE_UPLOAD_RETRY_WAIT 10ms
#define CAMERA_STARTER_FIRMWARE_UPLOAD_RETRIES 3
#define CAMERA_CLOSE_RETRY_WAIT 10ms
#define CAMERA_CLOSE_RETRIES 3

#define IS_SET_EVENT_TERMINATE_HANDLE_THREADS IS_SET_EVENT_USER_DEFINED_BEGIN + 1
static_assert(IS_SET_EVENT_TERMINATE_HANDLE_THREADS <= IS_SET_EVENT_USER_DEFINED_END);
#define IS_SET_EVENT_TERMINATE_CAPTURE_THREADS IS_SET_EVENT_USER_DEFINED_BEGIN + 2
static_assert(IS_SET_EVENT_TERMINATE_CAPTURE_THREADS <= IS_SET_EVENT_USER_DEFINED_END);

namespace uEyeWrapper
{
    void uploadProgressHandlerBar(uEyeCameraInfo camera, std::chrono::milliseconds duration, progress_state &state);

    // allocates and deallocates image buffers
    // keeps a reverse mapping from pointers to memory id, to be used in resolving memory id to get image info
    template <typename H>
    class imageMemoryManager : protected std::map<char *, INT>
    {
    public:
        imageMemoryManager(const H &);
        void initialize();
        void cleanup();

        INT getID(char *) const;

        ~imageMemoryManager();

    private:
        UEYE_API_CALL_PROTO();
        const H &_consumer_handle;
    };

    // TODO: what callbacks? image, capture status change, errors in async loops?, conn/reconn?
    template <imageColorMode M, imageBitDepth D>
    class uEyeHandle
    {
        // private:
        //     // can only be constructed by openCamera method
        //     uEyeHandle(uEyeCameraInfo& camera, std::function<void(int, std::string)>);

    public:
        typedef typename std::conditional_t<M == imageColorMode::MONO,
                                            typename std::conditional_t<D == imageBitDepth::i8, sln::PixelY_8u, sln::PixelY_16u>,     // MONO
                                            typename std::conditional_t<D == imageBitDepth::i8, sln::PixelRGB_8u, sln::PixelRGB_16u>> // RGB
            typedPixelT;
        typedef std::function<void(int, std::string, std::chrono::time_point<std::chrono::system_clock>)> captureStatusCallbackT;

        uEyeHandle(uEyeCameraInfo, captureStatusCallbackT = nullptr, std::function<void(uEyeCameraInfo, std::chrono::milliseconds, progress_state &)> = uploadProgressHandlerBar);
        ~uEyeHandle();

        template <captureType C>
        uEyeCaptureHandle<uEyeHandle<M, D>, C> getCaptureHandle(typename uEyeCaptureHandle<uEyeHandle<M, D>, C>::imageCallbackT);

        const uEyeCameraInfo &camera;
        // const double &FPS;
        // const bool &freerun_active;
        const std::tuple<int, int> &resolution;
        const sensorType &sensor;

        double setFPS(double);
        void setWhiteBalance(whiteBalance);
        void setWhiteBalance(int); // kelvin
        const captureErrors &errorStats;

    private:
        UEYE_API_CALL_PROTO();

        HIDS handle;

        uEyeCameraInfo _camera;
        // double _FPS;
        // bool _freerun_active;
        std::tuple<int, int> _resolution; // {width, height}
        sensorType _sensor;

        const typename std::underlying_type_t<decltype(M)> _channels;
        const typename std::underlying_type_t<decltype(D)> _bit_depth;
        const INT _uEye_color_mode;

        const size_t _concurrency;

        imageMemoryManager<uEyeHandle<M, D>> _memory_manager;

        captureErrors _error_stats;

        std::thread _capture_status_observer_executor;
        void _SPAWN_capture_status_observer();
        captureStatusCallbackT captureStatusCallback;

        void _open_camera(std::function<void(uEyeCameraInfo, std::chrono::milliseconds, progress_state &)> = uploadProgressHandlerBar);
        void _populate_sensor_info();
        void _init_events();
        void _setup_capture_to_memory();

        void _set_AutoControl_default();
        void _set_WhiteBalance_kelvin(unsigned int);
        void _set_WhiteBalance_AUTO(bool = true);
        unsigned int _set_WhiteBalance_colorModel_default();

        std::tuple<int, std::string> _get_last_error_msg() const;

        void _close_camera();
        // void _cleanup_memory(); // will be handled by destruction of memory manager
        void _stop_threads();
        void _cleanup_events();

        friend class imageMemoryManager<uEyeHandle<M, D>>;
        template <typename H, captureType C>
        friend class uEyeCaptureHandle;
    };
}