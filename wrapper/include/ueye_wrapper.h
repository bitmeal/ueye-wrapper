/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 * 
 * Copyright (C) 2020, Arne Wendt
 * 
 * */


#pragma once

#include <stdint.h>

#ifdef _WIN32
	#include <windows.h>
#else
	typedef uint32_t DWORD;
#endif

#include <tuple>
#include <vector>
#include <string>
#include <map>

#include <opencv2/opencv.hpp>

#define CAMERA_LIST_NONE_ADDITIONAL 0
#define CAMERA_LIST_WITH_CONNECTION_INFO 0x80
//#define CAMERA_LIST_WITH_SENSOR_INFO 0x40

#define CAMERA_AUTOCONF_IP "0.0.0.0/0"
#define CAMERA_SET_IP_MAX_RETRYS 3

#define IMAGE_MONO          0x08
#define IMAGE_MONO_8_INT    0x09
//#define IMAGE_MONO_16_INT     0x0A
#define IMAGE_MONO_32_F     0x0C
#define IMAGE_BGR           0x80
#define IMAGE_BGR_8_INT     0x90
#define IMAGE_BGR_32_F      0xC0

#define CAMERA_AUTO_OPTIMAL_CLK_MAX_FPS 0x10
#define CAMERA_AUTO_STARTER_FIRMWARE_UPLOAD 0x20
#define CAMERA_AUTO_STARTER_FIRMWARE_UPLOAD_PROGRESSBAR 0x40
#define CAMERA_AUTO_EXIT_HANDLE 0x80

#define CAMERA_STARTER_FIRMWARE_AUTO_UPLOAD_RETRYS 3

#define IMAGE_BUFFER_SIZE 3
#define ALLOC_MEM_TRYS 10
#define ACTIVATE_MEM_TRYS 10
#define FREE_MEM_TRYS 500
#define UNLOCK_MEM_TRYS 10
#define LOCK_MEM_TRYS 10
#define RETRY_PAUSE_MS 2

#define CLOCK_TUNING_MSEC 4000


class uEyeWrapper
{
    public:
    enum class connType {undefined, USB, ETH};
    enum class sensorType {undefined, MONO, BGR};
    struct uEyeCam
    {
        struct _info
        {
            std::string modelName;
            std::string serialNo;
            connType connection;
            std::tuple<int, int> sensorResolution;
            sensorType colorMode;
        };
        struct _config
        {
            std::string IP;
            bool isIPautoconf;
            DWORD devId;
        };

        DWORD camId;
        bool inUse;
        _info info;
        _config config;

        friend std::ostream& operator<<(std::ostream&, const uEyeCam&);
    };
    typedef std::vector<uEyeCam> cameraList;
    typedef unsigned int LIST_OPTIONS;
    typedef unsigned int IMAGE_OPTIONS;
    typedef unsigned int CAMERA_OPTIONS;

    class uEyeHandle
    {
        public:

        struct errorStats
        {
            int total;
            int bufferNoMem;
            int bufferLocked;
            int driverOutOfBuffers;
            int deviceTimeout;
            int deviceETHBufferOverrun;
            int deviceMissedImage;
            errorStats();

            friend std::ostream& operator<<(std::ostream&, const errorStats&);
            friend bool operator == (const errorStats&, const errorStats&);
            friend bool operator != (const errorStats&, const errorStats&);
        };

        uEyeHandle();
        ~uEyeHandle();
        uEyeCam camera;
        void getImage(cv::Mat&);
        double setFPS(double);
        void setTriggered();
        void setFreerun();
        void trigger();
        errorStats getErrors();
        void resetErrorCounters();
        size_t resizeBuffer(size_t);

        private:
        IMAGE_OPTIONS colorMode;
        //cv::Mat img;
        int handle;
        int width;
        int height;
        int bits_per_chanel;
        int chanels;
        int offset_per_px;
        int offset_per_chanel;
        //int memID[IMAGE_BUFFER_SIZE];
	    //char* pMem[IMAGE_BUFFER_SIZE];
        typedef std::map<int, char*> bufferMap;
        bufferMap buffers;
    	//void* pMemVoid;
	    //void* pPrevMemVoid;
        int maxPxlClk;
        double maxFrameRate;
        double FPS;
        bool freerun;
        
        size_t resizeBuffer(size_t, bool);
        size_t resizeBufferNOTHROW(size_t);
        void _cleanup();
        
        friend uEyeWrapper;
    };

    private:
    static void setCameraIP(uEyeCam&, const DWORD&, const DWORD&, int = 0);
    static void _showProgress(int, bool*, bool*);


    public:
    uEyeWrapper();
    static cameraList getCameraList(LIST_OPTIONS = CAMERA_LIST_WITH_CONNECTION_INFO); // | CAMERA_LIST_WITH_SENSOR_INFO);
    static void getCameraConnectionInfo(uEyeCam&);
    static void getCameraConnectionInfo(cameraList&);
    static void setCameraIP(uEyeCam&, const std::string&);
    static void setCameraIPRangeStaticAuto(cameraList&, const std::string&, bool = false);
    static void setCameraIPAutoConf(uEyeCam&);
    static void setCameraIPAutoConf(cameraList&);
    static void setCameraAutoIPRange(uEyeCam&, const std::string&, const std::string&);
    static void setCameraAutoIPRange(cameraList&, const std::string&, const std::string&);

    static void openCamera(uEyeHandle&, uEyeCam&,
        IMAGE_OPTIONS = IMAGE_BGR_32_F,
        CAMERA_OPTIONS =    CAMERA_AUTO_STARTER_FIRMWARE_UPLOAD |
                            CAMERA_AUTO_STARTER_FIRMWARE_UPLOAD_PROGRESSBAR |
                            CAMERA_AUTO_OPTIMAL_CLK_MAX_FPS);
   
    void showErrorReport();

    ~uEyeWrapper();
};