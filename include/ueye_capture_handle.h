#pragma once

#include "wrapper_helpers.h"
#include "wrapper_types.h"
namespace uEyeWrapper
{
    template <typename H, captureType C>
    class uEyeCaptureHandle;
}
#include "ueye_handle.h"

#include <selene/img/common/Types.hpp>
#include <selene/img/pixel/PixelTypeAliases.hpp>
#include <selene/img/typed/ImageView.hpp>
// #include <selene/img/typed/ImageViewAliases.hpp>
#include <selene/img_ops/Algorithms.hpp>

#include <BS_thread_pool.hpp>

#include <type_traits>

#include <functional>
#include <chrono>
#include <thread>

namespace uEyeWrapper
{
    template <typename H, captureType C>
    class uEyeCaptureHandle
    {
    public:
        typedef sln::MutableImageView <typename H::typedPixelT> typedImageViewT;
        typedef sln::ConstantImageView<typename H::typedPixelT> constTypedImageViewT;
        // image, timestamp, monotonic sequence counter, id
        // typedef std::function<void(constTypedImageViewT, std::chrono::time_point<std::chrono::system_clock>, size_t, size_t)> imageCallbackT;
        typedef std::function<void(typedImageViewT, std::chrono::time_point<std::chrono::system_clock>, size_t, size_t)> imageCallbackT;

        uEyeCaptureHandle() = delete;
        uEyeCaptureHandle(const H &, imageCallbackT);
        ~uEyeCaptureHandle();

        // disable for captureType::LIVE
        template <typename enable_SFINAE = void>
        auto trigger(bool = false) -> std::enable_if_t<C == captureType::TRIGGER, enable_SFINAE>;
        // void trigger();

    private:
        imageCallbackT imageCallback;
        // select implementation based on capture type (dynamic selection; is value not typename)
        void _start_capture();

        std::thread _image_dispatcher_executor;
        void _SPAWN_image_dispatcher();
        void _stop_threads();

        BS::thread_pool _pool;

        // stop live and triggered
        void _stop_capture();

        UEYE_API_CALL_PROTO();
        const H &_camera_handle;
    };
}