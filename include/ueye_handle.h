#pragma once

#include "wrapper_types.h"
#include <functional>
#include <chrono>


// #include <selene/img/dynamic/DynImageView.hpp>

namespace uEyeWrapper
{
    void uploadProgressHandlerBar(uEyeCameraInfo camera, std::chrono::milliseconds duration, progress_state &state);

    // callbacks: image, capture status change, errors in async loops, conn/reconn?
    template <colorMode T>
    class uEyeHandle
    {
        // private:
        //     // can only be constructed by openCamera method
        //     uEyeHandle(uEyeCameraInfo& camera, std::function<void(int, std::string)>);

    public:
        uEyeHandle(uEyeCameraInfo, std::function<void(int, std::string)> = nullptr, std::function<void(uEyeCameraInfo, std::chrono::milliseconds, progress_state &)> = uploadProgressHandlerBar);
        ~uEyeHandle();

        const uEyeCameraInfo &camera;
        // const double &FPS;
        // const bool &freerun_active;
        const std::tuple<int, int> &resolution;
        const sensorType &sensor;

        double setFPS(double);
        void setWhiteBalance(whiteBalance);
        void setWhiteBalance(int); // kelvin

    private:
        HIDS handle;

        uEyeCameraInfo _camera;
        // double _FPS;
        // bool _freerun_active;
        std::tuple<int, int> _resolution; // {width, height}
        sensorType _sensor;

        std::function<void(int, std::string)> errorCallback;

        void _open_camera(std::function<void(uEyeCameraInfo, std::chrono::milliseconds, progress_state &)> = uploadProgressHandlerBar);
        void _populate_sensor_info();
        void _setup_memory();

        void _set_AutoControl_default();
        void _set_WhiteBalance_kelvin(unsigned int);
        void _set_WhiteBalance_AUTO(bool = true);
        unsigned int _set_WhiteBalance_colorModel_default();

        void _close_camera();
        void _cleanup_memory();
    };

    // std::function<void(const sln::ConstantDynImageView&)>
    // class uEyeImageAcquisitionHandle
    // {
    //     private:
    //         uEyeImageAcquisitionHandle(HIDS handle, std::function<void(T)> imgCallback, std::function<void(int, std::string)> errorInfoCallback);

    //     public:
    //         ~uEyeImageAcquisitionHandle();

    //         void startVideo(double FPS);
    //         void stopVideo();

    //         void trigger();
    // };
}