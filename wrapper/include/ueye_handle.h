#pragma once

#include "types.h"

namespace uEyeWrapper
{
    class uEyeImageAcquisitionHandle
    {

    };


    // callbacks: image, capture status change, errors in async loops, conn/reconn?
    class uEyeHandle
    {
        public:
            uEyeHandle(uEyeCameraInfo camera);
            ~uEyeHandle();
            const uEyeCameraInfo camera;

            void startTriggered(bool external = false);
            void startVideo(int FPS);
            void stopVideo();

            void trigger();

        private:
            int handle;
            int width;
            int height;
            int bits_per_chanel;
            int chanels;
            int offset_per_px;
            int offset_per_chanel;

            double FPS;
            bool freerun;
            
            size_t resizeBuffer(size_t, bool);
            size_t resizeBufferNOTHROW(size_t);
            void _cleanup();
            
            friend openCamera;
    };
}