/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 * 
 * Copyright (C) 2020, Arne Wendt
 * 
 * */


#include <iostream>
#include <exception>
#include <thread>

#include <argh.h>
#include <ueye_wrapper.h>

#define DEFAULT_FPS 15.
#define DEFAULT_PAUSE 10
#define DEFAULT_IMAGE_MODE "RGB32F"


int main(int argc, char** argv)
{
  auto cmdl = argh::parser(argc, argv, argh::parser::SINGLE_DASH_IS_MULTIFLAG);
  cmdl.add_params({"--ID", "-r", "-f", "-p", "-M"});


  int camID;
  cmdl("--ID", 0) >> camID;
  if(camID == 0)
  {
    std::cout << "give camera ID with --ID parameter!" << std::endl;
    return EXIT_FAILURE;
  }

  std::cout << "camera ID " << camID << std::endl;

  std::string mode;
  uEyeWrapper::IMAGE_OPTIONS imgMode;

  mode = cmdl("-M", DEFAULT_IMAGE_MODE).str();

  if(mode == "RGB32F")
  {
    imgMode = IMAGE_BGR_32_F;
    std::cout << "color mode IMAGE_BGR_32_F" << std::endl;
  }
  else if(mode == "RGB8")
  {
    imgMode = IMAGE_BGR_8_INT;
    std::cout << "color mode IMAGE_BGR_8_INT" << std::endl;
  }
  else if(mode == "M32F")
  {
    imgMode = IMAGE_MONO_32_F;
    std::cout << "color mode IMAGE_MONO_32_F" << std::endl;
  }
  else if(mode == "M8")
  {
    imgMode = IMAGE_MONO_8_INT;
    std::cout << "color mode IMAGE_MONO_8_INT" << std::endl;
  }
  else
  {
    std::cout << "could not identify color mode to set! Options are [RGB32F, RGB8, M32F, M8]" << std::endl;
    return EXIT_FAILURE;
  }
  

  double fps;
  cmdl({"-r", "-f"}, DEFAULT_FPS) >> fps;
  std::cout << fps << " FPS" << std::endl;

  int pause;
  cmdl({"-p"}, DEFAULT_PAUSE) >> pause;
  std::cout << "\"inter grab\" pause " << pause << "ms" << std::endl;


  uEyeWrapper::cameraList camList = uEyeWrapper::getCameraList(CAMERA_LIST_WITH_CONNECTION_INFO);

  auto camera = std::find_if(camList.begin(), camList.end(), [camID](uEyeWrapper::uEyeCam camera){return camera.camId == camID;});

  if(camera == camList.end())
  {
    std::cout << "camera with ID " << camID << " not found!" << std::endl;
    return EXIT_FAILURE;
  }

  uEyeWrapper::uEyeHandle handle;
  uEyeWrapper::openCamera(handle, *camera, IMAGE_BGR_32_F);
  handle.setFPS(fps);

  handle.resizeBuffer(5);

  handle.resetErrorCounters();
  uEyeWrapper::uEyeHandle::errorStats errorStats = handle.getErrors();


  cv::Mat img;

  while(true)
  {
    try
    {
      handle.getImage(img);

      if(errorStats != handle.getErrors()) //new errors
      {
        std::cout << "Camera ID (" << camID << ") ERRORS:" << std::endl;
        std::cout << errorStats << std::endl;

        errorStats = handle.getErrors();
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(pause));
    }
    catch (const std::exception& e)
    {
      std::cout << e.what() << std::endl;
      return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}