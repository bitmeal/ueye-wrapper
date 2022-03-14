/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 * 
 * Copyright (C) 2020, Arne Wendt
 * 
 * */

#include <fmt/core.h>
#include <fmt/ranges.h>
#include <fmt/color.h>

#include <plog/Log.h>
#include <plog/Init.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Appenders/ColorConsoleAppender.h>
#define UEYE_WRAPPER_LOG_LEVEL_DEFAULT warning

#include "wrapper_helpers.h"
#include "ueye_wrapper.h"
#include "ip_helpers.h"

namespace uEyeWrapper
{
    // "global" concurrency configuration
    size_t concurrency = 3;

    static plog::ColorConsoleAppender<plog::TxtFormatter> plogCCA;
    static auto *logger = plog::get() == nullptr ? &(plog::init(plog::UEYE_WRAPPER_LOG_LEVEL_DEFAULT, &plogCCA)) : plog::get();

    plog::Logger<0>& getLogger()
    {
        return *(plog::get());
    }

    std::tuple<connectionType, std::string, bool> getCameraConnectionInfo(DWORD deviceId)
    {
        // deduce camera connection type from IP capabilities
        UINT uCaps = 0;
        int nret = is_IpConfig(deviceId, {0, 0, 0, 0, 0, 0}, IPCONFIG_CMD_QUERY_CAPABILITIES, &uCaps, sizeof(UINT));
        PLOG_DEBUG << fmt::format("is_IpConfig capabilities: {:#010x}", uCaps);
        if (nret == IS_NOT_SUPPORTED ||
            (nret == IS_SUCCESS && !(
                                       (uCaps & IPCONFIG_CAP_PERSISTENT_IP_SUPPORTED) ||
                                       (uCaps & IPCONFIG_CAP_AUTOCONFIG_IP_SUPPORTED))))
        {
            return {connectionType::USB, "", false};
        }

        if (nret == IS_SUCCESS && ((uCaps & IPCONFIG_CAP_PERSISTENT_IP_SUPPORTED) ||
                                   (uCaps & IPCONFIG_CAP_AUTOCONFIG_IP_SUPPORTED)))
        {
            // get IP address and info
            if ((uCaps & IPCONFIG_CAP_PERSISTENT_IP_SUPPORTED) == IPCONFIG_CAP_PERSISTENT_IP_SUPPORTED)
            {
                UEYE_ETH_IP_CONFIGURATION ipCfg;
                std::memset(&ipCfg, 0, sizeof(UEYE_ETH_IP_CONFIGURATION));

                UEYE_API_CALL(is_IpConfig, {(INT)deviceId, {0, 0, 0, 0, 0, 0}, IPCONFIG_CMD_GET_PERSISTENT_IP, &ipCfg, sizeof(UEYE_ETH_IP_CONFIGURATION)}, "fetching camera IP config failed, even though it should be supported");

                if (
                    ipCfg.ipAddress.dwAddr != 0 ||
                    !(uCaps & IPCONFIG_CAP_AUTOCONFIG_IP_SUPPORTED))
                {
                    std::string ipAddress = ip_bytes_to_string(ip_u32_little_endian_to_bytes(ipCfg.ipAddress.dwAddr)) + "/" + std::to_string(u32_to_cidr(ip_bytes_to_u32(ip_u32_little_endian_to_bytes(ipCfg.ipSubnetmask.dwAddr))));

                    // is static IP
                    return {connectionType::ETH, ipAddress, false};
                }
            }

            // should get here, only if static IP is either not supported
            // or static is 0.0.0.0/0 and autoconfig is supported - which equals enabled autoconfig
            if (uCaps & IPCONFIG_CAP_AUTOCONFIG_IP_SUPPORTED)
            {
                UEYE_ETH_AUTOCFG_IP_SETUP ipCfg;
                std::memset(&ipCfg, 0, sizeof(UEYE_ETH_AUTOCFG_IP_SETUP));

                UEYE_API_CALL(is_IpConfig, {(INT)deviceId, {0, 0, 0, 0, 0, 0}, IPCONFIG_CMD_GET_AUTOCONFIG_IP_BYDEVICE, &ipCfg, sizeof(UEYE_ETH_AUTOCFG_IP_SETUP)}, "fetching camera IP config failed, even though it should be supported");

                std::string ipAddressRange =
                    ip_bytes_to_string(ip_u32_little_endian_to_bytes(ipCfg.ipAutoCfgIpRangeBegin.dwAddr)) + ":" +
                    ip_bytes_to_string(ip_u32_little_endian_to_bytes(ipCfg.ipAutoCfgIpRangeEnd.dwAddr));

                return {connectionType::ETH, ipAddressRange, true};
            }
        }

        // else connection stays undefined
        throw std::runtime_error("could not determine connection type/info for camera with device id: " + std::to_string(deviceId));
    }

    cameraList getCameraList()
    {
        cameraList camList; // = cameraList(0);
        UEYE_CAMERA_LIST *clPtr;
        int numCams;

        UEYE_API_CALL(is_GetNumberOfCameras, {&numCams}, "failed to get number of cameras");

        PLOG_INFO << fmt::format("Found {} cameras", numCams);

        if (numCams == 0)
        {
            return camList;
        }

        // Create new list with suitable size
        clPtr = (UEYE_CAMERA_LIST *)new BYTE[sizeof(DWORD) + numCams * sizeof(UEYE_CAMERA_INFO)];
        clPtr->dwCount = numCams;

        UEYE_API_CALL(is_GetCameraList, {clPtr}, "failed to get list of cameras", [&]()
                      { delete clPtr; });

        for (unsigned int i = 0; i < clPtr->dwCount; ++i)
        {
            const auto connectionInfo = getCameraConnectionInfo(clPtr->uci[i].dwDeviceID);
            const auto [connection, ipAddress, ipAutoConf] = connectionInfo;

            camList.push_back(/*(uEyeCameraInfo)*/ {
                clPtr->uci[i].dwCameraID,
                clPtr->uci[i].dwDeviceID,

                clPtr->uci[i].FullModelName,
                clPtr->uci[i].SerNo,

                !clPtr->uci[i].dwInUse,

                connection,
                ipAddress,
                ipAutoConf

                // {0,0},
                // sensorType::undefined,
            });

            PLOG_INFO << fmt::format(
                "camera with device id {}: {} [#{}] @{}{} ({})",
                camList.back().deviceId,
                camList.back().modelName,
                camList.back().serialNo,
                camList.back().connection == connectionType::USB ? "USB" : camList.back().IP,
                camList.back().isIPautoconf ? " (autoconf)" : "",
                camList.back().canOpen ? "can open" : "cannot open");
        }

        delete clPtr;

        return camList;
    }

    template <imageColorMode M, imageBitDepth D>
    uEyeHandle<M, D> openCamera(const uEyeCameraInfo camera, std::function<void(int, std::string, std::chrono::time_point<std::chrono::system_clock>)> errorCallback, std::function<void(uEyeCameraInfo, std::chrono::milliseconds, progress_state &)> uploadProgressHandler)
    {
        // PLOG_INFO << fmt::format(
        //     "opening camera {}: {} [#{}]",
        //     camera.deviceId,
        //     camera.modelName,
        //     camera.serialNo);

        return uEyeHandle<M, D>(camera, errorCallback, uploadProgressHandler);
    }
    template uEyeHandle<uEye_MONO_8> openCamera<uEye_MONO_8>(const uEyeCameraInfo, std::function<void(int, std::string, std::chrono::time_point<std::chrono::system_clock>)>, std::function<void(uEyeCameraInfo, std::chrono::milliseconds, progress_state &)>);
    template uEyeHandle<uEye_RGB_8> openCamera<uEye_RGB_8>(const uEyeCameraInfo, std::function<void(int, std::string, std::chrono::time_point<std::chrono::system_clock>)>, std::function<void(uEyeCameraInfo, std::chrono::milliseconds, progress_state &)>);
    template uEyeHandle<uEye_MONO_16> openCamera<uEye_MONO_16>(const uEyeCameraInfo, std::function<void(int, std::string, std::chrono::time_point<std::chrono::system_clock>)>, std::function<void(uEyeCameraInfo, std::chrono::milliseconds, progress_state &)>);
    template uEyeHandle<uEye_RGB_16> openCamera<uEye_RGB_16>(const uEyeCameraInfo, std::function<void(int, std::string, std::chrono::time_point<std::chrono::system_clock>)>, std::function<void(uEyeCameraInfo, std::chrono::milliseconds, progress_state &)>);

}
