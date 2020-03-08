/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 * 
 * Copyright (C) 2020, Arne Wendt
 * 
 * */


#include "ueye_wrapper.h"

//#include <stdlib.h>
#include <exception>
#include <iostream>
#include <algorithm>
#include <deque>

#include <chrono>
#include <thread>

#include <ueye.h>
#include "ip_helpers.h"


uEyeWrapper::uEyeWrapper(){}


uEyeWrapper::cameraList uEyeWrapper::getCameraList(LIST_OPTIONS options)
{
    int nret;
    cameraList camList = cameraList(0);
	UEYE_CAMERA_LIST* clPtr;
	int numCams;

    nret = is_GetNumberOfCameras(&numCams);
	if (nret != IS_SUCCESS)
	{
		throw std::runtime_error("could not find any cameras; is_GetNumberOfCameras() returned with code " + std::to_string(nret));
	}
	if (numCams == 0)
	{
		return camList;
	}

	// Create new list with suitable size
	clPtr = (UEYE_CAMERA_LIST*) new BYTE [sizeof (DWORD) + numCams * sizeof (UEYE_CAMERA_INFO)];
	clPtr->dwCount = numCams;

    nret = is_GetCameraList(clPtr);
	if (nret != IS_SUCCESS)
	{
		throw std::runtime_error("failed to get list of cameras; is_GetCameraList() returned with code " + std::to_string(nret));
        delete clPtr;
	}
	
	for (unsigned int i=0; i < clPtr->dwCount; ++i)
	{
		camList.push_back(/*(uEyeCam)*/{
            /*.camId = */ clPtr->uci[i].dwCameraID,
            /*.inUse = */ clPtr->uci[i].dwInUse != 0 ? true : false,
            /*.info = *//*(uEyeCam::_info)*/ {
                /*.modelName = */ clPtr->uci[i].FullModelName,
                /*.serialNo = */ clPtr->uci[i].SerNo,
                /*.connection = */ connType::undefined,
                /*.sensorResolution = */ {0,0},
                /*.colorMode = */ sensorType::undefined
            },
            /*.config = *//*(uEyeCam::_config)*/ {
                /*.IP = */ "",
                /*.isIPautoconf = */ false,
                /*.devId = */ clPtr->uci[i].dwDeviceID
            }
        });

        /*
        // get camera type
        CAMINFO ciObj;
        nret = is_GetCameraInfo(clPtr->uci[i].dwDeviceID, &ciObj);
        if (nret != IS_SUCCESS)
        {
            throw std::runtime_error("failed to load camera info; is_GetCameraInfo() returned with code " + std::to_string(nret) + " for dynamic device Id " + std::to_string(clPtr->uci[i].dwDeviceID));
            delete clPtr;
        }

        connType type = (connType) undefined;
        
        if ((ciObj.Type & IS_INTERFACE_TYPE_USB) == IS_INTERFACE_TYPE_USB)
            type = USB;
        if ((ciObj.Type & IS_INTERFACE_TYPE_USB3) == IS_INTERFACE_TYPE_USB3)
            type = USB3;
        if ((ciObj.Type & IS_INTERFACE_TYPE_ETH) == IS_INTERFACE_TYPE_ETH)
            type = ETH;

        camList.back().info.connection = type;
        */
	}

    if ((options & CAMERA_LIST_WITH_CONNECTION_INFO) == CAMERA_LIST_WITH_CONNECTION_INFO)      
    {
        getCameraConnectionInfo(camList);
    }

    delete clPtr;

    return camList;
}


void uEyeWrapper::getCameraConnectionInfo(uEyeWrapper::uEyeCam& cam)
{
    // deduce camera connection type from IP capabilities        
    unsigned int uCaps = 0;
    int nret = is_IpConfig(cam.config.devId, {0,0,0,0,0,0}, IPCONFIG_CMD_QUERY_CAPABILITIES, &uCaps, sizeof(UINT));
    
    if (nret == IS_NOT_SUPPORTED || 
        (nret == IS_SUCCESS && (
        (uCaps & IPCONFIG_CAP_PERSISTENT_IP_SUPPORTED) != IPCONFIG_CAP_PERSISTENT_IP_SUPPORTED
        &&
        (uCaps & IPCONFIG_CAP_AUTOCONFIG_IP_SUPPORTED) !=
        IPCONFIG_CAP_AUTOCONFIG_IP_SUPPORTED
        ))
        )
    {
        cam.info.connection = connType::USB;
        return;
    }        


    if (nret == IS_SUCCESS && (
        (uCaps & IPCONFIG_CAP_PERSISTENT_IP_SUPPORTED) == IPCONFIG_CAP_PERSISTENT_IP_SUPPORTED ||
        (uCaps & IPCONFIG_CAP_AUTOCONFIG_IP_SUPPORTED) == IPCONFIG_CAP_AUTOCONFIG_IP_SUPPORTED
        ))
    {
        cam.info.connection = connType::ETH;
                
        // get IP address and info
        if ((uCaps & IPCONFIG_CAP_PERSISTENT_IP_SUPPORTED) == IPCONFIG_CAP_PERSISTENT_IP_SUPPORTED)
        {
            UEYE_ETH_IP_CONFIGURATION ipCfg;
            std::memset(&ipCfg, 0, sizeof(UEYE_ETH_IP_CONFIGURATION));

            nret = is_IpConfig(
                (DWORD)cam.config.devId,
                {0,0,0,0,0,0},
                IPCONFIG_CMD_GET_PERSISTENT_IP,
                &ipCfg,
                sizeof(UEYE_ETH_IP_CONFIGURATION));

            	if (nret != IS_SUCCESS)
            	{
		            throw std::runtime_error("fetching camera IP config failed, even though it should be supported; is_IpConfig() returned with code " + std::to_string(nret));
	            }
                
            if (
                ipCfg.ipAddress.dwAddr != 0 ||
                ((uCaps & IPCONFIG_CAP_AUTOCONFIG_IP_SUPPORTED) != IPCONFIG_CAP_AUTOCONFIG_IP_SUPPORTED)
            )
            {
                cam.config.IP = ip_bytes_to_string(ip_u32_little_endian_to_bytes(ipCfg.ipAddress.dwAddr)) + "/" + std::to_string(u32_to_cidr(ip_bytes_to_u32(ip_u32_little_endian_to_bytes(ipCfg.ipSubnetmask.dwAddr))));

                // RETURN HERE IF IS STATIC IP ADDRESS
                return;
            }
        }
        
        // should get here, only if static IP is either not supported
        // or static is 0.0.0.0/0 and autoconfig is supported - which equals enabled autoconfig
        if ((uCaps & IPCONFIG_CAP_AUTOCONFIG_IP_SUPPORTED) == IPCONFIG_CAP_AUTOCONFIG_IP_SUPPORTED)
        {
            UEYE_ETH_AUTOCFG_IP_SETUP ipCfg;
            std::memset(&ipCfg, 0, sizeof(UEYE_ETH_AUTOCFG_IP_SETUP));

            nret = is_IpConfig(
                (DWORD)cam.config.devId,
                {0,0,0,0,0,0},
                IPCONFIG_CMD_GET_AUTOCONFIG_IP_BYDEVICE,
                &ipCfg,
                sizeof(UEYE_ETH_AUTOCFG_IP_SETUP));

            if (nret != IS_SUCCESS)
            {
		        throw std::runtime_error("fetching camera IP config failed, even though it should be supported; is_IpConfig() returned with code " + std::to_string(nret));
	        }

            cam.config.isIPautoconf = true;
   
            cam.config.IP = ip_bytes_to_string(ip_u32_little_endian_to_bytes(ipCfg.ipAutoCfgIpRangeBegin.dwAddr)) + ":" +
                            ip_bytes_to_string(ip_u32_little_endian_to_bytes(ipCfg.ipAutoCfgIpRangeEnd.dwAddr));
        }

    }

    // else connection stays undefined
}


void uEyeWrapper::getCameraConnectionInfo(uEyeWrapper::cameraList& camList)
{
    for (auto camIt = camList.begin(); camIt != camList.end(); ++camIt)
        getCameraConnectionInfo(*camIt);
}


void uEyeWrapper::setCameraIP(uEyeWrapper::uEyeCam& cam, const DWORD& ip, const DWORD& netmask, int retry_counter)
{
    unsigned int uCaps = 0;
    int nret = is_IpConfig(cam.config.devId, {0,0,0,0,0,0}, IPCONFIG_CMD_QUERY_CAPABILITIES, &uCaps, sizeof(UINT));

    if (nret != IS_SUCCESS)
    {
        throw std::runtime_error("fetching camera IP config failed, even though it should be supported; is_IpConfig() returned with code " + std::to_string(nret));
    }
    
    if ((uCaps & IPCONFIG_CAP_PERSISTENT_IP_SUPPORTED) != IPCONFIG_CAP_PERSISTENT_IP_SUPPORTED)
    {
        throw std::runtime_error("selected camera with cameraID " + std::to_string(cam.camId) + " does not support setting a static IP address; is_IpConfig() returned code " + std::to_string(uCaps) + " to describe the capabilities");
    }

    UEYE_ETH_IP_CONFIGURATION ipCfg;
    std::memset(&ipCfg, 0, sizeof(UEYE_ETH_IP_CONFIGURATION));

    ipCfg.ipAddress.dwAddr = ip;
    ipCfg.ipSubnetmask.dwAddr = netmask;

    nret = is_IpConfig(
        (DWORD)cam.config.devId,
        {0,0,0,0,0,0},
        IPCONFIG_CMD_SET_PERSISTENT_IP,
        &ipCfg,
        sizeof(UEYE_ETH_IP_CONFIGURATION)
        );

    if (nret != IS_SUCCESS)
    {
        throw std::runtime_error("setting IP on camera with cameraID " + std::to_string(cam.camId) + " failed; is_IpConfig() returned with code " + std::to_string(nret));
    }

    //update IP from camera
    getCameraConnectionInfo(cam);
    if(
        ip_bytes_to_u32_little_endian(ip_string_to_bytes(get_address_wo_netmask(cam.config.IP))) != ip ||
        ip_bytes_to_u32_little_endian(ip_u32_to_bytes(cidr_to_u32(get_cidr_netmask(cam.config.IP)))) != netmask
    )
    {
        if (retry_counter >= CAMERA_SET_IP_MAX_RETRYS)
            throw std::runtime_error("setting IP was reported as success by driver, cameras' current IP and IP to be set do not match!");
    
        ++retry_counter;
        #ifdef INFO_MSGS
            std::cout << "IP fetched from camera does not match the IP to be set in camera. driver reported success though. Retrying... Try #" << retry_counter << std::endl;
        #endif
        setCameraIP(cam, ip, netmask, retry_counter);
    }


}


void uEyeWrapper::setCameraIP(uEyeWrapper::uEyeCam& cam, const std::string& cidrIP)
{
    if (!ip_valid_cidr(cidrIP))
        throw std::runtime_error("IP is not valid. Include netmask in CIDR notation!");
    
    DWORD ip = ip_bytes_to_u32_little_endian(ip_string_to_bytes(get_address_wo_netmask(cidrIP)));
    DWORD netmask = ip_bytes_to_u32_little_endian(ip_u32_to_bytes(cidr_to_u32(get_cidr_netmask(cidrIP))));

    setCameraIP(cam, ip, netmask);
}


void uEyeWrapper::setCameraIPRangeStaticAuto(uEyeWrapper::cameraList& camList, const std::string& cidrIP, bool overwriteStatic)
{
    if (!ip_valid_cidr(cidrIP))
        throw std::runtime_error("IP is not valid. Include netmask in CIDR notation!");
    
    ipBytes ipB = ip_string_to_bytes(get_address_wo_netmask(cidrIP));
    DWORD netmask = ip_bytes_to_u32_little_endian(ip_u32_to_bytes(cidr_to_u32(get_cidr_netmask(cidrIP))));


    for (auto camIt = camList.begin(); camIt != camList.end(); ++camIt)
    {        
        // get connection info if not set on camera object
        if (camIt->info.connection == connType::undefined)
            getCameraConnectionInfo(*camIt);
        
        // do not try to set an IP on a USB camera
        if (camIt->info.connection != connType::ETH)
            continue;

        // camera has a static/persistent ip?
        if (!camIt->config.isIPautoconf && !overwriteStatic)
            continue;

        // don't throw on individual cameras! this should be robust but undeterministic
        try
        {
            setCameraIP(*camIt, (DWORD)ip_bytes_to_u32_little_endian(ipB), netmask);
            ipB.by4++;
        }
        catch(const std::exception& e)
        {
            std::cerr << e.what() << '\n';
        }
    }
}


void uEyeWrapper::setCameraIPAutoConf(uEyeWrapper::uEyeCam& cam)
{
    setCameraIP(cam, CAMERA_AUTOCONF_IP);
}


void uEyeWrapper::setCameraIPAutoConf(uEyeWrapper::cameraList& camList)
{
    for (auto camIt = camList.begin(); camIt != camList.end(); ++camIt)
        setCameraIPAutoConf(*camIt);
}


void uEyeWrapper::setCameraAutoIPRange(uEyeWrapper::uEyeCam& cam, const std::string& rangeBeginCIDRIP, const std::string& rangeEndCIDRIP)
{
    if (!ip_valid_cidr(rangeBeginCIDRIP))
        throw std::runtime_error("range start IP is not valid. Include netmask in CIDR notation!");
    
    if (!ip_valid_cidr(rangeEndCIDRIP))
        throw std::runtime_error("range end IP is not valid. Include netmask in CIDR notation!");

    DWORD ipRangeBegin = ip_bytes_to_u32_little_endian(ip_string_to_bytes(get_address_wo_netmask(rangeBeginCIDRIP)));
    DWORD ipRangeEnd = ip_bytes_to_u32_little_endian(ip_string_to_bytes(get_address_wo_netmask(rangeEndCIDRIP)));


    // get connection info if not set on camera object
    if (cam.info.connection == connType::undefined)
        getCameraConnectionInfo(cam);

    if (cam.info.connection != connType::ETH && !cam.config.isIPautoconf)
        throw std::runtime_error("Single camera to set an automatic IP on, is either not an ethernet camera or not in auto config mode!");

    // NOT CHECKING CAPABILITIES HERE ANYMORE! CAPABILITIES SHOULD BE SUFFICIENTLY CHECED AND PROCESSED BY getCameraConnectionInfo()
    
    UEYE_ETH_AUTOCFG_IP_SETUP ipAutoCfg;
    memset(&ipAutoCfg, 0, sizeof(UEYE_ETH_AUTOCFG_IP_SETUP)); 
    ipAutoCfg.ipAutoCfgIpRangeBegin.dwAddr = ipRangeBegin;
    ipAutoCfg.ipAutoCfgIpRangeEnd.dwAddr = ipRangeEnd;
    int nret =  is_IpConfig(
                    cam.config.devId,
                    {0,0,0,0,0,0},
                    IPCONFIG_CMD_SET_AUTOCONFIG_IP_BYDEVICE,
                    &ipAutoCfg,
                    sizeof(UEYE_ETH_AUTOCFG_IP_SETUP)
                );

    if (nret != IS_SUCCESS)
    {
        throw std::runtime_error("setting auto IP on camera ID(" + std::to_string(cam.camId) + " ); is_IpConfig() returned with code " + std::to_string(nret));
    }
}


void uEyeWrapper::setCameraAutoIPRange(uEyeWrapper::cameraList& camList, const std::string& rangeBeginCIDRIP, const std::string& rangeEndCIDRIP)
{
    if (!ip_valid_cidr(rangeBeginCIDRIP))
        throw std::runtime_error("range start IP is not valid. Include netmask in CIDR notation!");
    
    if (!ip_valid_cidr(rangeEndCIDRIP))
        throw std::runtime_error("range end IP is not valid. Include netmask in CIDR notation!");

    for (auto camIt = camList.begin(); camIt != camList.end(); ++camIt)
    {        
        // get connection info if not set on camera object
        if (camIt->info.connection == connType::undefined)
            getCameraConnectionInfo(*camIt);
        
        // do not try to set an IP on a USB camera or a camera not in autoconfig mode
        if (camIt->info.connection != connType::ETH ||
            !camIt->config.isIPautoconf)
            continue;


        // don't throw on individual cameras! this should be robust but undeterministic
        try
        {
            setCameraAutoIPRange(*camIt, rangeBeginCIDRIP, rangeEndCIDRIP);
        }
        catch(const std::exception& e)
        {
            std::cerr << e.what() << '\n';
        }
    }

}

void uEyeWrapper::_showProgress(int ms, bool* term_flag, bool* fail_flag)
{   
    const int barWidth = 20;

    const auto start = std::chrono::high_resolution_clock::now();
    auto elapsed = [start](){
        return (
            std::chrono::duration_cast<std::chrono::milliseconds>(
                (std::chrono::duration<float>) (std::chrono::high_resolution_clock::now() - start)
                )
            ).count();
        };
    

    do {
        std::cout << "\r";
        double progress = (double)elapsed() / (double)ms;
        std::cout << "[";
        int pos = barWidth * progress;
        for (int i = 0; i < barWidth; ++i) {
            if (i < pos) std::cout << "=";
            else if (i == pos) std::cout << ">";
            else std::cout << " ";
        }
        std::cout << "] " << int(progress * 100.0) << " %";
        std::cout.flush();

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    } while (elapsed() < ms && !*term_flag);

    if(*fail_flag)
    {
        std::cout << " - [FAILED]" << std::endl;
        return;
    }

    // show 100%
    std::cout << "\r[" << std::string(barWidth, '=') << "] 100%" << std::endl;
}

void uEyeWrapper::openCamera(uEyeHandle& camHandle, uEyeCam& cam, IMAGE_OPTIONS imgOpts, CAMERA_OPTIONS camOpts)
{
    if (camHandle.handle != 0 || camHandle.buffers.size() != 0)
        throw std::logic_error("camera handle already in use!");

    HIDS hCam = cam.camId;
    int nret;
    
    int uploadRetrys = 0;
    while(++uploadRetrys <= CAMERA_STARTER_FIRMWARE_AUTO_UPLOAD_RETRYS)
    {
        nret = is_InitCamera (&hCam, NULL);
        
        if (nret != IS_SUCCESS)
        {
            //Prüfen, ob eine GigE uEye SE neue Starter Firmware benötigt
            if (nret == IS_STARTER_FW_UPLOAD_NEEDED)
            {

                if ((camOpts & CAMERA_AUTO_STARTER_FIRMWARE_UPLOAD) != CAMERA_AUTO_STARTER_FIRMWARE_UPLOAD)
                    throw std::runtime_error("opening camera with cameraID " + std::to_string(cam.camId) + " failed; A new starter firmware has to be uploaded but you disabled auto upload!");

                //Zeit für das Aktualisieren der Starter Firmware ermitteln
                int msUpdate;
                nret = is_GetDuration (hCam, IS_SE_STARTER_FW_UPLOAD, &msUpdate);
                if (nret != IS_SUCCESS)
                    throw std::runtime_error("could not request duration for starter firmware upload; is_GetDuration(,IS_SE_STARTER_FW_UPLOAD,) returned with code " + std::to_string(nret));
                
                std::thread t_progress;
                bool t_terminate = false;
                bool t_failed = false;
                if ( (camOpts & CAMERA_AUTO_STARTER_FIRMWARE_UPLOAD_PROGRESSBAR) == CAMERA_AUTO_STARTER_FIRMWARE_UPLOAD_PROGRESSBAR )
                {
                    std::cout << "Loading starter firmware to camera with ID(" << cam.camId << ")" << std::endl;
                    t_progress = std::thread(_showProgress, msUpdate + 1000, &t_terminate, &t_failed); //add 1000ms to estimated upload time
                }
                
                //Beim Initialisieren neue Starter Firmware hochladen
                hCam = hCam | IS_ALLOW_STARTER_FW_UPLOAD;
                nret = is_InitCamera (&hCam, NULL);

                if (nret != IS_SUCCESS)
                    t_failed = true; // operation failed

                t_terminate = true; //terminate progress bar

                if ( (camOpts & CAMERA_AUTO_STARTER_FIRMWARE_UPLOAD_PROGRESSBAR) == CAMERA_AUTO_STARTER_FIRMWARE_UPLOAD_PROGRESSBAR )
                    t_progress.join();

                if (nret == IS_CANT_OPEN_DEVICE || nret == IS_DEVICE_ALREADY_PAIRED)
                {
                    #ifdef INFO_MSGS
                        std::cout << "could not open camera after firmware upload; try #" << uploadRetrys << " -> retrying" << std::endl;
                    #endif
                    is_ExitCamera(hCam); //try destroying handel, if possible
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue; //try again; at least reopening the device
                }

                if (nret != IS_SUCCESS)
                    throw std::runtime_error("opening camera with cameraID " + std::to_string(cam.camId) + " and updating starter firmware failed; is_InitCamera() returned with code " + std::to_string(nret));
                
                break; //opening worked
            }
            else
                throw std::runtime_error("opening camera with cameraID " + std::to_string(cam.camId) + " failed; is_InitCamera() returned with code " + std::to_string(nret));
        }
        else // opened successfuly
            break;
        
    }

    if(uploadRetrys > CAMERA_STARTER_FIRMWARE_AUTO_UPLOAD_RETRYS)
        throw std::runtime_error("opening camera with cameraID " + std::to_string(cam.camId) + " failed after " + std::to_string(CAMERA_STARTER_FIRMWARE_AUTO_UPLOAD_RETRYS) + " attempts to upload starter firmware!");


    camHandle.handle = hCam;

    // we should have a handle on the camera now
    SENSORINFO sensorInf;
    nret = is_GetSensorInfo (camHandle.handle, &sensorInf);
    if (nret != IS_SUCCESS)
    {
        is_ExitCamera(hCam); // close handle and allocated memory
        throw std::runtime_error("could not retreive sensor information from camera with cameraID " + std::to_string(cam.camId) + "; is_GetSensorInfo() returned with code " + std::to_string(nret));
    }
    
    cam.info.sensorResolution = {sensorInf.nMaxWidth, sensorInf.nMaxHeight};
    switch(sensorInf.nColorMode)
    {
        case IS_COLORMODE_MONOCHROME:
            cam.info.colorMode = sensorType::MONO; break;
        case IS_COLORMODE_BAYER:
            cam.info.colorMode = sensorType::BGR; break;
    }

    camHandle.width = std::get<0>(cam.info.sensorResolution);
    camHandle.height = std::get<1>(cam.info.sensorResolution);


    // set color mode
    if ( (imgOpts & IMAGE_MONO_8_INT) == IMAGE_MONO_8_INT)
    {
        nret = is_SetColorMode(camHandle.handle, IS_CM_MONO8);
        camHandle.bits_per_chanel = 8;
        camHandle.chanels = 1;
        camHandle.offset_per_px = 0;
        camHandle.offset_per_chanel = 0;
        //camHandle.img = cv::Mat(camHandle.height, camHandle.width, CV_8UC1);
        #ifdef DEBUG_MSGS
            std::cout << "IMAGE_MONO_8_INT" << std::endl;
        #endif
    }

    if ( (imgOpts & IMAGE_MONO_32_F) == IMAGE_MONO_32_F)
    {
        nret = is_SetColorMode(camHandle.handle, IS_CM_MONO16);
        camHandle.bits_per_chanel = 16;
        camHandle.chanels = 1;
        camHandle.offset_per_px = 0;
        camHandle.offset_per_chanel = 0;
        //camHandle.img = cv::Mat(camHandle.height, camHandle.width, CV_16UC1);
        #ifdef DEBUG_MSGS
            std::cout << "IMAGE_MONO_32_F" << std::endl;
        #endif
    }

    if ( (imgOpts & IMAGE_BGR_8_INT) == IMAGE_BGR_8_INT)
    {
        nret = is_SetColorMode(camHandle.handle, IS_CM_BGRA8_PACKED);
        camHandle.bits_per_chanel = 8;
        camHandle.chanels = 3;
        camHandle.offset_per_px = 8;
        camHandle.offset_per_chanel = 0;
        //camHandle.img = cv::Mat(camHandle.height, camHandle.width, CV_8UC3);
        #ifdef DEBUG_MSGS
            std::cout << "IMAGE_BGR_8_INT" << std::endl;
        #endif
    }

    if ( (imgOpts & IMAGE_BGR_32_F) == IMAGE_BGR_32_F)
    {
        nret = is_SetColorMode(camHandle.handle, IS_CM_BGRA12_UNPACKED);
        camHandle.bits_per_chanel = 12;
        camHandle.chanels = 3;
        camHandle.offset_per_px = 16;
        camHandle.offset_per_chanel = 4;
        //camHandle.img = cv::Mat(camHandle.height, camHandle.width, CV_32FC3);
        #ifdef DEBUG_MSGS
            std::cout << "IMAGE_BGR_32_F" << std::endl;
        #endif
    }
    
    if (nret != IS_SUCCESS)
    {
        is_ExitCamera(camHandle.handle); // close handle and allocated memory
        throw std::runtime_error("setting color mode for camera with cameraID " + std::to_string(cam.camId) + " failed; is_SetColorMode() returned with code " + std::to_string(nret));
    }

    camHandle.resizeBuffer(IMAGE_BUFFER_SIZE);

    /*
    //allocate and activate image memory
	for (int i = 0; i < IMAGE_BUFFER_SIZE; i++)
    {
		nret = is_AllocImageMem(hCam,
                camHandle.width,
                camHandle.height,
                (camHandle.bits_per_chanel + camHandle.offset_per_chanel) * camHandle.chanels + camHandle.offset_per_px,
                &(camHandle.pMem[i]),
                &(camHandle.memID[i])
            );
        if (nret != IS_SUCCESS)
        {
            // clean allocated memory
			for (int i_clean = 0; i_clean < i; i_clean++) {
				int trys = 0;
				while (is_FreeImageMem(camHandle.handle, *(camHandle.pMem + i_clean), *(camHandle.memID + i_clean)) != IS_SUCCESS && trys < FREE_MEM_TRYS) trys++;
			}
            is_ExitCamera(camHandle.handle); // close handle and allocated memory
            throw std::runtime_error("allocating memory for camera with cameraID " + std::to_string(cam.camId) + " failed; is_AllocImageMem() returned with code " + std::to_string(nret));
        }
	}
    #ifdef DEBUG_MSGS
	    std::cout << "memory allocated" << std::endl;
    #endif

	for (int i = 0; i < IMAGE_BUFFER_SIZE; i++)
    {
		nret = is_AddToSequence(camHandle.handle, camHandle.pMem[i], camHandle.memID[i]);
        if (nret != IS_SUCCESS)
        {
			camHandle._cleanup();
            throw std::runtime_error("building buffer sequence for camera with cameraID " + std::to_string(cam.camId) + " failed; is_AddToSequence() returned with code " + std::to_string(nret));
        }
	}
    #ifdef DEBUG_MSGS
    	std::cout << "buffer ready" << std::endl;
    #endif
    */

    // display mode: "display in RAM"
    nret = is_SetDisplayMode(camHandle.handle, IS_SET_DM_DIB);
    if (nret != IS_SUCCESS)
    {
		camHandle._cleanup();
        throw std::runtime_error("setting display mode to DIB on camera with cameraID " + std::to_string(cam.camId) + " failed; is_SetDisplayMode(, IS_SET_DM_DIB) returned with code " + std::to_string(nret));
    }


    //set parameters
	double enable = 1;
	double disable = 0;

    // set AUTO gain
	nret = is_SetAutoParameter(camHandle.handle, IS_SET_ENABLE_AUTO_GAIN, &enable, 0);
    if (nret != IS_SUCCESS)
    {
		camHandle._cleanup();
        throw std::runtime_error("setting AUTO gain on camera with cameraID " + std::to_string(cam.camId) + " failed; is_SetAutoParameter(, IS_SET_ENABLE_AUTO_GAIN,) returned with code " + std::to_string(nret));
    }

    // set AUTO WB
	nret = is_SetAutoParameter(camHandle.handle, IS_SET_ENABLE_AUTO_WHITEBALANCE, &enable, 0);
    if (nret != IS_SUCCESS)
    {
		camHandle._cleanup();
        throw std::runtime_error("setting AUTO whitebalance on camera with cameraID " + std::to_string(cam.camId) + " failed; is_SetAutoParameter(, IS_SET_ENABLE_AUTO_WHITEBALANCE,) returned with code " + std::to_string(nret));
    }

    #ifdef DEBUG_MSGS
    	std::cout << "auto parameters set" << std::endl;
    #endif


    // set triggering to software: use continuous image capturing or softrware triggered capture
    nret = is_SetExternalTrigger(camHandle.handle, IS_SET_TRIGGER_SOFTWARE);
    if (nret != IS_SUCCESS)
    {
		camHandle._cleanup();
        throw std::runtime_error("setting software triggering on camera with cameraID " + std::to_string(cam.camId) + " failed; is_SetExternalTrigger(, IS_SET_TRIGGER_SOFTWARE) returned with code " + std::to_string(nret));
    }

	nret = is_CaptureVideo(camHandle.handle, IS_DONT_WAIT);
    if (nret != IS_SUCCESS)
    {
		camHandle._cleanup();
        throw std::runtime_error("starting image capturing on camera with cameraID " + std::to_string(cam.camId) + " failed; is_CaptureVideo(, IS_DONT_WAIT) returned with code " + std::to_string(nret));
    }

    camHandle.freerun = true;
    #ifdef DEBUG_MSGS
    	std::cout << "started capture" << std::endl;
    #endif

    // set optimal pixel clock and max frame rate on windows, if desired
#ifdef _WIN32
    if( (camOpts | CAMERA_AUTO_OPTIMAL_CLK_MAX_FPS) == CAMERA_AUTO_OPTIMAL_CLK_MAX_FPS )
    {
        nret = is_SetOptimalCameraTiming (hCam, IS_BEST_PCLK_RUN_ONCE, CLOCK_TUNING_MSEC, &(camHandle.maxPxlClk), &(camHandle.maxFrameRate));
        if (nret != IS_SUCCESS)
        {
            camHandle._cleanup();
            throw std::runtime_error("setting optimal pixel clock on camera with cameraID " + std::to_string(cam.camId) + " failed; is_SetOptimalCameraTiming() returned with code " + std::to_string(nret));
        }

        #ifdef DEBUG_MSGS
            std::cout << "set pixel clock" << std::endl;
        #endif

        // try setting max fps; if it fails, it fails...
        nret = is_SetFrameRate(hCam, camHandle.maxFrameRate, &(camHandle.FPS));
        #ifdef DEBUG_MSGS
            std::cout << "set frame rate" << std::endl;
        #endif
    }
#endif

    // get actual framerate and write to camera handle
    nret = is_SetFrameRate(camHandle.handle, IS_GET_FRAMERATE, &(camHandle.FPS));
    if (nret != IS_SUCCESS)
    {
		camHandle._cleanup();
        throw std::runtime_error("requesting FPS on camera with cameraID " + std::to_string(cam.camId) + " failed; is_SetFrameRate(, IS_GET_FRAMERATE) returned with code " + std::to_string(nret));
    }

}

void uEyeWrapper::showErrorReport()
{
    int nret = is_SetErrorReport(0, IS_ENABLE_ERR_REP);
    if (nret != IS_SUCCESS)
    {
        throw std::runtime_error("enabling error report failed; is_SetErrorReport(, IS_ENABLE_ERR_REP) returned with code " + std::to_string(nret));
    }
}


uEyeWrapper::~uEyeWrapper() {}

uEyeWrapper::uEyeHandle::uEyeHandle() : handle(0) {}

uEyeWrapper::uEyeHandle::~uEyeHandle() {_cleanup();}

int uEyeWrapper::uEyeHandle::resizeBuffer(size_t size) { return resizeBuffer(size, true); }
int uEyeWrapper::uEyeHandle::resizeBufferNOTHROW(size_t size) { return resizeBuffer(size, false); }
int uEyeWrapper::uEyeHandle::resizeBuffer(size_t size, bool may_throw)
{
    auto freeMem = [this](int memID, char* memPtr) -> bool
    {
        int freeMemTrys = 0;
        while (
            is_FreeImageMem(handle, memPtr, memID) != IS_SUCCESS
            && freeMemTrys < FREE_MEM_TRYS) ++freeMemTrys;

        if (freeMemTrys == FREE_MEM_TRYS)
            return false;

        return true;
    };

    auto activateMem = [this](int memID, char* memPtr) -> bool
    {
        int activateMemTrys = 0;
        while (
            is_AddToSequence(handle, memPtr, memID) != IS_SUCCESS
            && activateMemTrys < ACTIVATE_MEM_TRYS)
            ++activateMemTrys;

        if (activateMemTrys == ACTIVATE_MEM_TRYS)
            return false;

        return true;
    };

    auto allocMem = [this](int& memID, char*& memPtr) -> bool
    {
        int allocMemTrys = 0;
        while (
            is_AllocImageMem(handle,
                width,
                height,
                (bits_per_chanel + offset_per_chanel) * chanels + offset_per_px,
                &memPtr,
                &memID) != IS_SUCCESS
            && allocMemTrys < ALLOC_MEM_TRYS
            )
            ++allocMemTrys;

        // tried too many times - continue
        if (allocMemTrys == ALLOC_MEM_TRYS)
            return false;

        return true;
    };

    if (size > buffers.size()) // expand buffer
    {
        //allocate image memory and store ID and address in buffer map
        for (size_t i = 0; i < (size - buffers.size()); i++)
        {
            int memID = 0;
            char* memPtr = NULL;

            if ( !allocMem(memID, memPtr))
                continue;

            if( !activateMem(memID, memPtr) )
                if ( !freeMem(memID, memPtr)  && may_throw)
                    throw std::runtime_error("allocating memory for camera with cameraID " + std::to_string(camera.camId) + " failed; image memory could not be activated and trying to free the unactivated memory failed " + std::to_string(FREE_MEM_TRYS) + " times");

            // add buffer to map, with its ID as key
            buffers[memID] = memPtr;
        }

    }
    else if (size < buffers.size()) // shrink buffer
    {
        // free memory and remove entry form map, while iterating over map
        // associative container iterators are invalidated for the removed element only!
        for (auto buffIt = buffers.rbegin(); buffIt != buffers.rend(); ++buffIt)
        {
            // freeMem() tries multiple times by itself; if successful, remove entry from map, if not, try next
            if (freeMem(buffIt->first, buffIt->second))
                buffers.erase(buffIt->first);

            if (buffers.size() == size)
                break;
        }
    }
    
    return buffers.size();
}

void uEyeWrapper::uEyeHandle::getImage(cv::Mat& out)
{
    // we have to copy our data anyways, as memory layout of ueye driver is incompatible with opencv representation
    // we try to get the latest active buffer, lock it, read our data and unlock it again
    char* pChImgMem;
    void* pImgMem;
    int nret;

    // get last active buffer
    // nret = is_GetImageMem(handle, &pImgMem);
    nret = is_GetActSeqBuf(handle, NULL, NULL, &pChImgMem);
    pImgMem = pChImgMem;
    if (nret != IS_SUCCESS)
    {
        throw std::runtime_error("getting image memory on camera with cameraID " + std::to_string(camera.camId) + " failed; is_GetImageMem() returned with code " + std::to_string(nret));
    }

    // lock buffer access
    for (int trys = 0; trys < LOCK_MEM_TRYS; trys++)
    {
	    nret = is_LockSeqBuf(handle, IS_IGNORE_PARAMETER, (char*)pImgMem);
        if (nret == IS_SUCCESS) break;
    }
    if (nret != IS_SUCCESS)
    {
        throw std::runtime_error("locking image memory buffer on camera with cameraID " + std::to_string(camera.camId) + " failed; is_LockSeqBuf() returned with code " + std::to_string(nret) + " after " + std::to_string(LOCK_MEM_TRYS) + "trys");
    }

    cv::Mat img;
    // get image into opencv matrix
    switch(chanels)
    {
        case 1:
            switch(bits_per_chanel)
            {
                case 8:
                    img = cv::Mat(height, width, CV_8UC1, (uint8_t*)pImgMem);
                    img.copyTo(out);
                    #ifdef DEBUG_MSGS
                        std::cout << "captured MONO 8" << std::endl;
                    #endif
                    break;
                case 16:
                    img = cv::Mat(height, width, CV_16UC1, (uint16_t*)pImgMem);
                    img.convertTo(out, CV_32FC1, 1./256.);
                    #ifdef DEBUG_MSGS
                        std::cout << "captured  MONO 16" << std::endl;
                    #endif
                    break;
            } break;

        case 3:
            switch(bits_per_chanel)
            {
                case 8:
                    img = cv::Mat(height, width, CV_8UC4, pImgMem);
                    cv::cvtColor(img, out, cv::COLOR_RGBA2RGB);
                    #ifdef DEBUG_MSGS
                        std::cout << "captured BGRA 8" << std::endl;
                    #endif
                    break;
                case 12:
                    img = cv::Mat(height, width, CV_16UC4, pImgMem);
                    cv::Mat imd;// = cv::Mat(height, width, CV_32FC4);
                    img.convertTo(imd, CV_32FC4, 1./16.); // 1/6 == 2^4 : align bit order by division. scaling from type conversion seems to be handled automatically
                    cv::cvtColor(imd, out, cv::COLOR_RGBA2RGB);
                    #ifdef DEBUG_MSGS
                        std::cout << "captured BGRA 12" << std::endl;
                    #endif
                    break;
            } break;

            break;
    }


    // unlock buffer
    for (int trys = 0; trys < UNLOCK_MEM_TRYS; trys++)
    {
	    nret = is_UnlockSeqBuf(handle, IS_IGNORE_PARAMETER, (char*)pImgMem);
        if (nret == IS_SUCCESS) break;
    }
    if (nret != IS_SUCCESS)
    {
        throw std::runtime_error("unlocking image memory buffer on camera with cameraID " + std::to_string(camera.camId) + " failed; is_UnlockSeqBuf() returned with code " + std::to_string(nret) + " after " + std::to_string(UNLOCK_MEM_TRYS) + "trys");
    }
}

double uEyeWrapper::uEyeHandle::setFPS(double fps)
{
    int nret;
    
    double minTmg, minFPS, maxTmg, maxFPS, intervall;
    UINT clk;
    std::vector<UINT> clkList;

    // get FPS range
    nret = is_GetFrameTimeRange (handle, &minTmg, &maxTmg, &intervall);
    if (nret != IS_SUCCESS)
    {
        throw std::runtime_error("querying FPS range on camera with cameraID " + std::to_string(camera.camId) + " failed; is_GetFrameTimeRange() returned with code " + std::to_string(nret));
    }
    minFPS = 1/maxTmg;
    maxFPS = 1/minTmg;

    // FPS out of current pixel clocks range
    if(fps > maxFPS)
    {

        // get pixel clock
        nret = is_PixelClock(handle, IS_PIXELCLOCK_CMD_GET, (void*)&clk, sizeof(UINT));
        if (nret != IS_SUCCESS)
        {
            throw std::runtime_error("querying current pixel clock on camera with cameraID " + std::to_string(camera.camId) + " failed; is_PixelClock(, IS_PIXELCLOCK_CMD_GET,,) returned with code " + std::to_string(nret));
        }

        // get possible clocks
        
        // METHOD 1
        UINT clkRange[3];
        std::memset(clkRange, 0, sizeof(clkRange));
        nret = is_PixelClock(handle, IS_PIXELCLOCK_CMD_GET_RANGE, (void*)clkRange, sizeof(clkRange));
        if ((nret == IS_SUCCESS) && (clkRange[2] != 0)) // we got info on the possible pixel clocks
        {
            // push clock values to vector
            clkList.reserve((clkRange[1] - clkRange[0]) / clkRange[2] + 1);
            for(int n = 0; clkRange[0] + n*clkRange[2] <= clkRange[1]; ++n)
                clkList.push_back(clkRange[0] + n*clkRange[2]);

        }
        // METHOD 2
        else // got no info, as increment was 0 -> only a few discrete clocks available
        {
            UINT numClk = 0;
            int nretNum = is_PixelClock(handle, IS_PIXELCLOCK_CMD_GET_NUMBER, (void*)&numClk, sizeof(UINT));

            if (nretNum != IS_SUCCESS) // throw with all info, if not successful
            {
                throw std::runtime_error("failed to get pixel clock info on camera with cameraID " + std::to_string(camera.camId) + "; is_PixelClock(,IS_PIXELCLOCK_CMD_GET_RANGE) returned with code " + std::to_string(nret) + "and clock increment(" + std::to_string(clkRange[2]) + "), is_PixelClock(,IS_PIXELCLOCK_CMD_GET_NUMBER) returned with code " + std::to_string(nretNum));
            }

            if (numClk == 0)
            {
                throw std::runtime_error("failed to get pixel clock info on camera with cameraID " + std::to_string(camera.camId) + "; is_PixelClock(,IS_PIXELCLOCK_CMD_GET_NUMBER) returned 0 available clocks");
            }

            clkList = std::vector<UINT>(numClk, 0);
            nret = is_PixelClock(handle, IS_PIXELCLOCK_CMD_GET_LIST, (void*)clkList.data(), numClk * sizeof(UINT));
            if (nret != IS_SUCCESS)
            {
                throw std::runtime_error("failed to get pixel clock info on camera with cameraID " + std::to_string(camera.camId) + "; is_PixelClock(,IS_PIXELCLOCK_CMD_GET_LIST) returned with code " + std::to_string(nret));
            }
        }

        // vector has available clocks sets; pop elements from front til we reach the first element greater than our current clock
        auto nextClk = std::find_if(clkList.begin(), clkList.end(), [clk](UINT clkElem){return clkElem > clk;});
        std::deque<UINT> clkOpts(nextClk, clkList.end());


        // loop throug pixel clocks till we can set desired FPS
        while(fps > maxFPS && !clkOpts.empty()) //desired FPS within range
        {
            // set clock
            nret = is_PixelClock(handle, IS_PIXELCLOCK_CMD_SET, (void*)&(clkOpts.front()), sizeof(UINT));
            if (nret != IS_SUCCESS)
            {
                throw std::runtime_error("setting pixel clock on camera with cameraID " + std::to_string(camera.camId) + " failed; is_PixelClock(,IS_PIXELCLOCK_CMD_SET,,) returned with code " + std::to_string(nret));
            }

            #ifdef DEBUG_MSGS
                std::cout << "set pixel clock to " << std::to_string(clkOpts.front()) << std::endl;
            #endif
            clkOpts.pop_front();

            
            // get FPS range
            nret = is_GetFrameTimeRange (handle, &minTmg, &maxTmg, &intervall);
            if (nret != IS_SUCCESS)
            {
                throw std::runtime_error("querying FPS range on camera with cameraID " + std::to_string(camera.camId) + " failed; is_GetFrameTimeRange() returned with code " + std::to_string(nret));
            }
            minFPS = 1/maxTmg;
            maxFPS = 1/minTmg;
        }

    }

    //set PFS after (when necessary) adjusting pixel clock
    nret = is_SetFrameRate(handle, fps, &(this->FPS));
    if (nret != IS_SUCCESS)
    {
        throw std::runtime_error("setting FPS on camera with cameraID " + std::to_string(camera.camId) + " failed; is_SetFrameRate() returned with code " + std::to_string(nret));
    }

    #ifdef DEBUG_MSGS
        std::cout << "set FPS to " << std::to_string(this->FPS) << std::endl;
    #endif
    return this->FPS;
}

void uEyeWrapper::uEyeHandle::setTriggered()
{
    is_ForceTrigger(handle); // just in case: stop all running capturing operations. return value of no real interest - nice try though
    int nret = is_StopLiveVideo(handle, IS_FORCE_VIDEO_STOP);
    if (nret != IS_SUCCESS)
    {
        throw std::runtime_error("setting triggered mode on camera with cameraID " + std::to_string(camera.camId) + " failed; is_StopLiveVideo(, IS_FORCE_VIDEO_STOP) returned with code " + std::to_string(nret));
    }

    freerun = false;

}

void uEyeWrapper::uEyeHandle::setFreerun()
{
    int nret = is_CaptureVideo(handle, IS_DONT_WAIT);
    if (nret != IS_SUCCESS)
    {
        throw std::runtime_error("setting freerun mode on camera with cameraID " + std::to_string(camera.camId) + " failed; is_CaptureVideo(, IS_DONT_WAIT) returned with code " + std::to_string(nret));
    }

    freerun = true;

    setFPS(FPS);
}

void uEyeWrapper::uEyeHandle::trigger()
{
    if (freerun)
        throw std::logic_error("could not trigger camera! camera is in freerun mode!");
    
    int nret = is_FreezeVideo(handle, IS_WAIT);
    if (nret != IS_SUCCESS)
    {
        throw std::runtime_error("capturing triggered image on camera with cameraID " + std::to_string(camera.camId) + " failed; is_FreezeVideo(, IS_WAIT) returned with code " + std::to_string(nret));
    }
}


uEyeWrapper::uEyeHandle::errorStats uEyeWrapper::uEyeHandle::getErrors()
{
    errorStats errors;
    UEYE_CAPTURE_STATUS_INFO captureStatusInfo;
    
    int nret = is_CaptureStatus(handle, IS_CAPTURE_STATUS_INFO_CMD_GET, (void*)& captureStatusInfo, sizeof(captureStatusInfo));

    if (nret != IS_SUCCESS)
    {
        throw std::runtime_error("retreiving error stats on camera with cameraID " + std::to_string(camera.camId) + " failed; is_CaptureStatus() returned with code " + std::to_string(nret));
    }

    errors.total = captureStatusInfo.dwCapStatusCnt_Total;
    errors.bufferNoMem =captureStatusInfo.adwCapStatusCnt_Detail[IS_CAP_STATUS_API_NO_DEST_MEM];
    errors.bufferLocked = captureStatusInfo.adwCapStatusCnt_Detail[IS_CAP_STATUS_API_IMAGE_LOCKED];
    errors.driverOutOfBuffers = captureStatusInfo.adwCapStatusCnt_Detail[IS_CAP_STATUS_DRV_OUT_OF_BUFFERS];
    errors.deviceTimeout = captureStatusInfo.adwCapStatusCnt_Detail[IS_CAP_STATUS_DEV_TIMEOUT];
    errors.deviceETHBufferOverrun = captureStatusInfo.adwCapStatusCnt_Detail[IS_CAP_STATUS_ETH_BUFFER_OVERRUN];
    errors.deviceMissedImage = captureStatusInfo.adwCapStatusCnt_Detail[IS_CAP_STATUS_DEV_MISSED_IMAGES];

    return errors;
}

void uEyeWrapper::uEyeHandle::resetErrorCounters()
{
    int nret = is_CaptureStatus(handle, IS_CAPTURE_STATUS_INFO_CMD_RESET, NULL, 0);

    if (nret != IS_SUCCESS)
    {
        throw std::runtime_error("resetting error stats on camera with cameraID " + std::to_string(camera.camId) + " failed; is_CaptureStatus(,IS_CAPTURE_STATUS_INFO_CMD_RESET,,) returned with code " + std::to_string(nret));
    }
}

void uEyeWrapper::uEyeHandle::_cleanup()
{
    resizeBufferNOTHROW(0);
    if (handle != 0)
    {
        #ifdef DEBUG_MSGS
            std::cout << "closing camera..." << std::endl;
        #endif
        int trys = 0;
        while (is_ExitCamera(handle) != IS_SUCCESS && trys < FREE_MEM_TRYS) trys++;
    }
}

std::ostream& operator<<(std::ostream& os, const uEyeWrapper::uEyeCam& cam)
{
    os << "Camera: ";
    os << "ID(" << cam.camId << "), ";
    os << (cam.inUse ? "used, " : "free, ");
    
    switch(cam.info.connection)
    {
        case uEyeWrapper::connType::undefined:
            os << "unknown connection type!";
            break;
        case uEyeWrapper::connType::USB:
            os << "USB";
            break;
        case uEyeWrapper::connType::ETH:
            os << cam.config.IP << " ";
            os << (cam.config.isIPautoconf ? "(autoconfig)" : "(static)");
            break;
    }
    os << std::endl;
    return os;
}

uEyeWrapper::uEyeHandle::errorStats::errorStats() :
    total(0),
    bufferNoMem(0),
    bufferLocked(0),
    driverOutOfBuffers(0),
    deviceTimeout(0),
    deviceETHBufferOverrun(0),
    deviceMissedImage(0)
    {};


std::ostream& operator<<(std::ostream& os, const uEyeWrapper::uEyeHandle::errorStats& errors)
{
    
    os << "CAP_STATUS_API_NO_DEST_MEM: " << errors.bufferNoMem << std::endl;
    os << "CAP_STATUS_API_IMAGE_LOCKED: " << errors.bufferLocked << std::endl;
    os << "CAP_STATUS_DRV_OUT_OF_BUFFERS: " << errors.driverOutOfBuffers << std::endl;
    os << "CAP_STATUS_DEV_TIMEOUT:  " << errors.deviceTimeout << std::endl;
    os << "CAP_STATUS_ETH_BUFFER_OVERRUN: " << errors.deviceETHBufferOverrun << std::endl;
    os << "CAP_STATUS_DEV_MISSED_IMAGES: " << errors.deviceMissedImage << std::endl;

    return os;
}

bool operator == (const uEyeWrapper::uEyeHandle::errorStats& lhs, const uEyeWrapper::uEyeHandle::errorStats& rhs)
{
    return (
        ( lhs.total ==                   rhs.total                   ) &&
        ( lhs.bufferNoMem ==             rhs.bufferNoMem             ) &&
        ( lhs.bufferLocked ==            rhs.bufferLocked            ) &&
        ( lhs.driverOutOfBuffers ==      rhs.driverOutOfBuffers      ) &&
        ( lhs.deviceTimeout ==           rhs.deviceTimeout           ) &&
        ( lhs.deviceETHBufferOverrun ==  rhs.deviceETHBufferOverrun  ) &&
        ( lhs.deviceMissedImage ==       rhs.deviceMissedImage       )
    );
}

bool operator != (const uEyeWrapper::uEyeHandle::errorStats& lhs, const uEyeWrapper::uEyeHandle::errorStats& rhs)
{
    return !(lhs == rhs);
}
