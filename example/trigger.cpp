#include "ueye_wrapper.h"
using namespace std::chrono_literals;

#include <fmt/core.h>

#include <selene/img/interop/ImageToDynImage.hpp>
#include <selene/img_ops/Clone.hpp>
#include <selene/img_io/IO.hpp>
#include <selene/base/io/FileWriter.hpp>
#include <selene/img_ops/Algorithms.hpp>
#include <selene/img_ops/ImageConversions.hpp>
// #include <selene/img_io/png/Write.hpp>

int main(int argc, char const *argv[])
{
    try
    {
        // set library loglevel
        uEyeWrapper::getLogger().setMaxSeverity(plog::debug);

        // set max concurrency: number of buffers and thread pool size
        uEyeWrapper::concurrency = 5;
        auto cameras = uEyeWrapper::getCameraList();

        if (cameras.size())
        {
            auto camera = uEyeWrapper::openCamera<uEye_RGB_16>(cameras.front());
            camera.setWhiteBalance(uEyeWrapper::whiteBalance::halogen);

            // { // settle auto parameters
            //     camera.setFPS(10);
            //     auto capture = camera.getCaptureHandle<uEyeWrapper::captureType::LIVE>(
            //         [](auto...) { /* noop */
            //         });
            //     std::this_thread::sleep_for(1s);
            // }

            { // trigger manually
                auto capture = camera.getCaptureHandle<uEyeWrapper::captureType::TRIGGER>(
                    [](auto image, auto timestamp, auto seq, auto id)
                    {
                        // endian swap in place for 16bit PNG
                        sln::for_each_pixel(image,
                                              [](auto& px)
                                              {
                                                  for(size_t i = 0; i < px.nr_channels; i++)
                                                  {
                                                    px[i] = ((uint16_t)px[i] << 8) | ((uint16_t)px[i] >> 8);
                                                  }
                                              });

                        auto dynImg = sln::to_dyn_image_view(image);
                        sln::write_image(
                            dynImg,
                            sln::ImageFormat::PNG,
                            sln::FileWriter(fmt::format("./dev_test_{}.png", seq)));
                    });

                capture.trigger(true);
                std::this_thread::sleep_for(1s);
                capture.trigger(true);
            }
        }

        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "FAILED: " << e.what() << std::endl;
    }
}
