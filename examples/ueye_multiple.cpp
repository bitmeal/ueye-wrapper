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
#include <memory>

#include <ueye_wrapper.h>


int main(int argc, char** argv)
{
  uEyeWrapper::cameraList camList = uEyeWrapper::getCameraList(CAMERA_LIST_WITH_CONNECTION_INFO);
  for (auto it = camList.begin(); it != camList.end(); ++it)
    std::cout << *it << std::endl;

  std::vector<std::shared_ptr<uEyeWrapper::uEyeHandle>> handles;

  try
  {
    for(auto camIt = camList.begin(); camIt != camList.end(); ++camIt)
    {
      auto handle = std::make_shared<uEyeWrapper::uEyeHandle>();
      handles.push_back(handle);
      uEyeWrapper::openCamera(*(handles.back()), *camIt, IMAGE_BGR_32_F);
      (handles.back())->setFPS(15);
    }
  }
  catch (const std::exception& e)
  {
    std::cout << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  std::vector<cv::Mat> imgVec(handles.size());

  while(true)
  {
    auto imgIt = imgVec.begin();
    auto camIt = handles.begin();

    for(; camIt != handles.end(); ++camIt, ++imgIt)
    {
      (*camIt)->getImage(*imgIt);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

  }

  return EXIT_SUCCESS; //will never reach
}