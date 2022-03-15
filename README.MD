# uEye C++ wrapper
A simple C++ wrapper for image capture with asynchronous image handling around the iDS uEye *C*-Interface. Full control over the camera functions is traded for automatic setup and sane *auto-mode* configuration. **Channel count and bit depth are strongly typed** and images are passed to the consuming application as *selene* `sln::MutableImageView` (allowing to transform the data in place) with *zero copy*.

## desing goals & application field
This wrapper was written to be used in distributed image processing applications for clusters of many cameras. Utilizing 40GbE Network links and many processor cores of the used hardware, thus concurrently preprocessing and encoding/serializing/transferring images, while asynchronously triggering (at library level) nearly synchronous image acquisitions across a cluster.

Main objectives are:
* use in long running applications with changing environmental conditions
* extremely simple interface (sensible default and auto-configuration)
* asynchronous image capture API
* sane timestamping and information for sequence ordering
* concurrent processing of acquired images (not missing frames due to expensive image encoding/serialization)
* *zero-copy* for low latency
* type-safety
* *OpenCV* compatibility (provided by *selene*)

> 📌 The first two design goals do (for us) currently translate to: Auto-configuration for all capture parameters except framerate and white-balance.

## usage
### prepare
Query for available cameras. You can only construct handles on a camera from the `uEyeCameraInfo` obtained by calling `getCameraList()`;
```C++
auto cameras = getCameraList();
```

### open a camera - get a handle
* Find your camera in the list of available cameras using STL algorithms; filtering on the info provided by `uEyeCameraInfo`. *The example simply opens the first one!* 
* Select your number of color channels as `imageColorMode::MONO` or `imageColorMode::RGB`, and the bit depth as `imageBitDepth::i8` or `imageBitDepth::i16`. *16 bit* color images are scaled to *16 bit "full scale"* from **only 12 bits provided by the camera**!
* Provide an **optional** callback to handle capture status changes and errors. Disable by omitting or passing `nullptr`
* You can supply an additional handler method to handle progress feedback for camera firmware upload. Providing none will display a progress bar in the attached terminal, replaced by a log message when *stdout* is no terminal. Disable by passing `nullptr`
> 📌 See below on how to configure concurrency for your image processing
```C++
auto camera = openCamera<uEye_MONO_8>(
    cameras.front(),
    [](int i, std::string msg, std::chrono::time_point<std::chrono::system_clock> timestamp) {
        // no action on capture status change
        return;
    }
);
```
Aliases/defines for the number of channels and bit depth are provided:
* `uEye_MONO_8`
* `uEye_RGB_8`
* `uEye_MONO_16`
* `uEye_RGB_16`

### configure camera
Only white balance and framerate can be configured by now; in line with the design goal of a high simplicity wrapper.

```C++
camera.setFPS(1);
camera.setWhiteBalance(whiteBalance::overcast);
```

### capture images 📸
Start image capturing and processing by requesting a `uEyeCaptureHandle` and attaching a callback method (or lambda). Capture handles are strongly typed on `captureType::LIVE` or `captureType::TRIGGER` to allow compile time sanity checks and implementation selection. Capture will start automatically for `LIVE` handles. Use the `getCaptureHandle::trigger()` method to trigger an image capture for `TRIGGER` handles. `getCaptureHandle::trigger(bool)` accepts a boolean parameter, indicating whether to wait for the trigger event to occur or not. **The example shows how to write an image to a `*.png` file using *selene*. 
> 📌 **16 bit PNG images may require an endian swap using the *selene* methods; check against your implementation/version!**
```C++
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
```
Images are passed as a mutable view on the memory region holding the image-data. **Image-data is not copied** and the callback function does **not own** the data! As long as the callback function does not return, the memory is locked for exclusive use and will not be overwritten by the driver. If your processing is time intensive, set your concurrency value accordingly.

### concurrency
The concurrency value determines how many image buffers are available to the driver as a ring-buffer, and how many threads are available to execute the supplied callback functions for acquired images. The default concurrency is *3*. Configure a new value before your call to `openCamera()`.
```C++
uEyeWrapper::concurrency = 5;
```
### cleanup
Let `uEyeCaptureHandle` and `uEyeHandle` of your camera get out of scope for automatic cleanup.

### logging
The library makes extensive use of *plog* for logging purposes. If you are using *plog* yourself, just init a logger and the library will reuse it. To set the libraries loglevel use:
```C++
uEyeWrapper::getLogger().setMaxSeverity(plog::debug);
```