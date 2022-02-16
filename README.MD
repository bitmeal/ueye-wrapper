# uEye C++ wrapper
This is a wrapper around the iDS uEye *C*-Interface, returning captured images as *OpenCV* matrices (`cv::Mat` object). The interface exposes simple, *high-level* functionality in favour of total control over the individual camera. *CMake*-files to use the wrapper in your project are included. 

**Additionally, a *Find.cmake* to find an installation of the *uEye-SDK* is included.**

*All CMake files and libraries are installable. **Windows and Linux supported!***

---

The repository aggregates four individual (partly dependent) projects. You can build everything at once from the main CMakeList.txt in the repositories root.


## uEye SDK CMake config
In `ueye-sdk-cmake`, `FindUEYE-SDK.cmake` is provided. This CMake-script will find an installation of the *uEye-SDK* on your machine and create CMake library-targets to link against in your project.

```cmake
find_package( UEYE-SDK )
find_package( Threads )

...

target_link_libraries( <your-target> ueye-sdk Threads::Threads )
```
32 and 64-bit versions are auto detected per your platform. The SDK is searched for in the default installation paths on Windows and Linux. If the location of the SDK cannot be determined, you have to point the find script in the right direction by using the variables listed below. When using a graphical interface for CMake, only missing variables will be show in non-advanced mode. With a default installation, or when adhering to **NIX* standard locations, only `UEYE-SDK` should be needed, to define a common root for headers an libraries.

* `UEYE-SDK`: defines a common root directory for library and headers. *Headers* may be in subdirectories `/.;/include;/usr/include;/usr/local/include` and *libraries* in `/.;/Lib;/lib;/lib64;/i386-linux-gnu;/x86_64-linux-gnu` (*depending on platform*)
* `UEYE-SDK_INCLUDE_DIRS`: sets the include directories directly
* `UEYE-SDK_LIBRARIES`: sets the libraries directly

## uEye wrapper
In `wrapper` provides the actual wrapper library. The library can be built and installed system wide, to be found by CMake. ***OpenCV is required.*** You may have to link against additional *OpenCV*-libraries for your application (`[${OpenCV_LIBS}]` below).
### add it to your project
Add the dependency to your CMake based project and include the headers, as follows
```cmake
find_package( ueye-wrapper REQUIRED )
...
target_link_libraries( <your-target> ueye-wrapper [${OpenCV_LIBS}])

```
```c++
#include <ueye_wrapper.h>
```
### working with cameras
The types you need to work with cameras are
* `uEyeWrapper::cameraList`: a list of cameras
* `uEyeWrapper::uEyeCam`: representation of a single cameras properties
* `uEyeWrapper::uEyeHandle`: interface to the camera

#### open a camera
(1) get a list of available cameras (GigE and USB), (2) select a camera from the list and (3) get a handle to interface the camera.

The example opens a camera by its `ID`, for `ID == 7`.

```c++
int camID = 7;
uEyeWrapper::cameraList camList = uEyeWrapper::getCameraList(CAMERA_LIST_WITH_CONNECTION_INFO);
auto camera = std::find_if(camList.begin(), camList.end(),
    [camID](uEyeWrapper::uEyeCam camera){
        return camera.camId == camID;
    });

uEyeWrapper::uEyeHandle handle;
uEyeWrapper::openCamera(handle, *camera, IMAGE_BGR_32_F);
```

When opening a camera you may specify the color-depth and number of channels for the to be captured images. Additionally you may control the upload of matching starter firmwares for your driver, enable automatic *pixel-clock tuning* (Windows only), and enable automatic destruction of driver handles and memory freeing when closing a camera.

Possible color modes are:
* `IMAGE_MONO_8_INT`
* `IMAGE_MONO_32_F`
* `IMAGE_BGR_8_INT`
* `IMAGE_BGR_32_F`

Additional configuration options include:
* `CAMERA_AUTO_OPTIMAL_CLK_MAX_FPS`: tune pixel-clock and FPS to max throughput *(Windows only)*
* `CAMERA_AUTO_STARTER_FIRMWARE_UPLOAD`: automatically upload camera firmware matching the driver version, if mismatching
* `CAMERA_AUTO_STARTER_FIRMWARE_UPLOAD_PROGRESSBAR`: show firmware upload progressbar in terminal
* `CAMERA_AUTO_EXIT_HANDLE`: automatically free resources on driver side, when exiting camera

*You may save some time by calling `uEyeWrapper::getCameraList(CAMERA_LIST_NONE_ADDITIONAL)`, but it is advised against. This will not probe the cameras for their connection method. The check has to be performed later in that case!*

#### configure a camera
The interface to a *live* camera is an instance of `uEyeHandle`. To get static information about a camera, use the info in a `uEyeCam`; info about the open camera can be read from `uEyeHandle::camera` variable.
---
Cameras are configured to use **auto exposure**, **auto white-balance** and **auto gain**. Methods for manual control are **not** implemented. *The idea for now is an  interface as simple as possible. You can indirectly control these parameters by setting you FPS.*

A camera can be configured with following, limited set of options:
* `setFPS(double)`: set your frames per second to desired value in freerun/video-mode; returns the real FPS the camera is operating at. if the pixel-clock of the camera has to be increased, this is done automatically for you
* `setTriggered()`: enable triggered image acquisition; default is **freerun**
* `setFreerun()`: enable freerunning trigger, with configured FPS
* `resizeBuffer(size_t)`: set number of buffers, used by the driver to capture images to

### get an image
        void trigger();
        void getImage(cv::Mat&);


### get error stats
You may get information about occurred errors from each camera. This is highly useful in tuning high throughput applications. To reset the stats and start from all counters set to 0, use `uEyeHandle::resetErrorCounters()`. Get a struct of type `uEyeHandle::errorStats`, containing all current errors, by calling `uEyeHandle::getErrors()`.




