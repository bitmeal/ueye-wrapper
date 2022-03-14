# uEye C++ wrapper
A simple C++ wrapper for image capture with asynchronous image handling around the iDS uEye *C*-Interface. Full control over the camera functions is traded for automatic setup and sane *auto-mode* configuration. Channel count and bit depth are strongly typed and images are handed over to the consuming application as *selene* `sln::ConstantImageView` without copying.

## quickstart example
* query for available cameras
* select camera by e.g. ID or serial number
* get a handle on the camera (open and configure it)
* get an capture handle to capture images

```C++
#include "ueye_wrapper.h"
using namespace std::chrono_literals;

#include <fmt/core.h>

#include <selene/img/interop/ImageToDynImage.hpp>
#include <selene/img_io/IO.hpp>
#include <selene/base/io/FileWriter.hpp>

int main(int argc, char const *argv[])
{
    auto cameras = uEyeWrapper::getCameraList();
    auto camera = uEyeWrapper::openCamera<uEye_MONO_8>(
        cameras.front(),
        [](int i, std::string msg, std::chrono::time_point<std::chrono::system_clock> timestamp) {
            // no action on capture status change
            return;
        }
    );
    
    camera.setFPS(1);

    auto capture = camera.getCaptureHandle<uEyeWrapper::captureType::LIVE>(
        [](auto image, auto timestamp, auto seq, auto id)
        {
            auto dynImg = sln::to_dyn_image_view(image);
            sln::write_image(
                dynImg,
                sln::ImageFormat::PNG,
                sln::FileWriter(fmt::format("./dev_test_{}.png", seq)));
        }
    );

    std::this_thread::sleep_for(10s);

    return 0;
}
```