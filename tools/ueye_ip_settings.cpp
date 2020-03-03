/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 * 
 * Copyright (C) 2020, Arne Wendt
 * 
 * */


#include <iostream>
#include <exception>

#include <argh.h>
#include <ueye_wrapper.h>
#include <ip_helpers.h>

enum opMode {setAuto, setAutoRange, setRange, none};

int main(int argc, char** argv)
{
    opMode mode = none;
    std::string startStatic;
    std::string startAuto;
    std::string endAuto;

    auto cmdl = argh::parser(argc, argv, argh::parser::SINGLE_DASH_IS_MULTIFLAG);
    cmdl.add_params({
            //"-A", "--set-auto",
            //"-l", "--list",
            //"-f", "--force", "-o", "--override",
            //"-c", "--list-check",
            "-b", "--auto-begin",
            "-e", "--auto-end",
            "-S", "--set-static-range"
        });

    bool override = cmdl[{"-f", "--force", "-o", "--override"}];
    if (cmdl[{"-A", "--set-auto"}])
    {
        mode = setAuto;
    
        startAuto = cmdl({"-b", "--auto-begin"}).str();
        endAuto = cmdl({"-e", "--auto-end"}).str();

        if (!startAuto.empty() && !endAuto.empty())
        {
            if(!ip_valid_cidr(startAuto))
            {
                std::cout << "parameter -b/--auto-begin does not matches regex: (\\d{1,3}\\.){3}\\d{1,3}/\\d{1,2}" << std::endl;
                std::cout << "please give an IPv4 address with CIDR subnet notation: XX.XX.XX.XX/YY" << std::endl;
                return EXIT_FAILURE;
            }

            if(!ip_valid_cidr(endAuto))
            {
                std::cout << "parameter -e/--auto-end does not matches regex: (\\d{1,3}\\.){3}\\d{1,3}/\\d{1,2}" << std::endl;
                std::cout << "please give an IPv4 address with CIDR subnet notation: XX.XX.XX.XX/YY" << std::endl;
                return EXIT_FAILURE;
            }

            mode = setAutoRange;
        }
    }

    startStatic = cmdl({"-S", "--set-static-range"}).str();
    if(!startStatic.empty())
    {
        if(!ip_valid_cidr(startStatic))
        {
            std::cout << "parameter -S/--set-static does not matches regex: (\\d{1,3}\\.){3}\\d{1,3}/\\d{1,2}" << std::endl;
            std::cout << "please give an IPv4 address with CIDR subnet notation: XX.XX.XX.XX/YY" << std::endl;
            return EXIT_FAILURE;
        }

        mode = setRange;
    }


    // get list of cameras
    uEyeWrapper::cameraList camList = uEyeWrapper::getCameraList(CAMERA_LIST_WITH_CONNECTION_INFO);

    // we shall display a list?
    if (cmdl[{"-l", "--list"}] || mode == none)
    {
        for (auto it = camList.begin(); it != camList.end(); ++it)
            std::cout << *it << std::endl;
    }

    try
    {
        switch(mode)
        {
            case setAuto:
                uEyeWrapper::setCameraIPAutoConf(camList);
                break;
            
            case setAutoRange:
                uEyeWrapper::setCameraIPAutoConf(camList);
                uEyeWrapper::setCameraAutoIPRange(camList, startAuto, endAuto);
                break;
            
            case setRange:
                uEyeWrapper::setCameraIPRangeStaticAuto(camList, startStatic, override);
                break;
        }
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }
    

    // we shall display a list again, to check operation
    if (cmdl[{"-c", "--list-check"}] && mode != none)
    {
        camList = uEyeWrapper::getCameraList(CAMERA_LIST_WITH_CONNECTION_INFO);
        for (auto it = camList.begin(); it != camList.end(); ++it)
            std::cout << *it << std::endl;
    }

    
    
    return EXIT_SUCCESS;
}