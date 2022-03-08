#include "ueye_wrapper.h"

int main(int argc, char const *argv[])
{
    auto cameras = uEyeWrapper::getCameraList();

    if(cameras.size())
    {
        auto camera = uEyeWrapper::openCamera<uEyeWrapper::colorMode::IMAGE_MONO_8_INT>(cameras.front(), [](int i, std::string msg){ /*noop*/ return;});
    }

    return 0;
}
