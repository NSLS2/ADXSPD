/*
 * Header file for the ADXSPD EPICS driver
 *
 * This file was initially generated with the help of the ADDriverTemplate:
 * https://github.com/NSLS2/ADDriverTemplate on 01/07/2025
 *
 * Author: Jakub Wlodek
 *
 * Copyright (c) : Brookhaven National Laboratory, 2025
 *
 */

// header guard
#ifndef ADXSPD_H
#define ADXSPD_H

// version numbers
#define ADXSPD_VERSION 0
#define ADXSPD_REVISION 0
#define ADXSPD_MODIFICATION 1

#include <cpr/cpr.h>
#include <epicsExit.h>
#include <epicsExport.h>
#include <epicsStdio.h>
#include <epicsString.h>
#include <epicsThread.h>
#include <epicsTime.h>
#include <iocsh.h>
#include <stdlib.h>
#include <zmq.h>

#include <cstddef>
#include <cstdio>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
#include <string>

#include "ADDriver.h"
#include "epicsThread.h"

using json = nlohmann::json;
using namespace std;

// Error message formatters
#define ERR(msg)                                       \
    if (this->getLogLevel() >= ADXSPD_LOG_LEVEL_ERROR) \
        printf("ERROR | %s::%s: %s\n", driverName, functionName, msg);

#define ERR_ARGS(fmt, ...)                             \
    if (this->getLogLevel() >= ADXSPD_LOG_LEVEL_ERROR) \
        printf("ERROR | %s::%s: " fmt "\n", driverName, functionName, __VA_ARGS__);

// Warning message formatters
#define WARN(msg)                                        \
    if (this->getLogLevel() >= ADXSPD_LOG_LEVEL_WARNING) \
        printf("WARNING | %s::%s: %s\n", driverName, functionName, msg);

#define WARN_ARGS(fmt, ...)                              \
    if (this->getLogLevel() >= ADXSPD_LOG_LEVEL_WARNING) \
        printf("WARNING | %s::%s: " fmt "\n", driverName, functionName, __VA_ARGS__);

// Info message formatters. Because there is no ASYN trace for info messages, we just use `printf`
// here.
#define INFO(msg)                                     \
    if (this->getLogLevel() >= ADXSPD_LOG_LEVEL_INFO) \
        printf("INFO | %s::%s: %s\n", driverName, functionName, msg);

#define INFO_ARGS(fmt, ...)                           \
    if (this->getLogLevel() >= ADXSPD_LOG_LEVEL_INFO) \
        printf("INFO | %s::%s: " fmt "\n", driverName, functionName, __VA_ARGS__);

// Debug message formatters
#define DEBUG(msg)                                     \
    if (this->getLogLevel() >= ADXSPD_LOG_LEVEL_DEBUG) \
    printf("DEBUG | %s::%s: %s\n", driverName, functionName, msg)

#define DEBUG_ARGS(fmt, ...)                           \
    if (this->getLogLevel() >= ADXSPD_LOG_LEVEL_DEBUG) \
        printf("DEBUG | %s::%s: " fmt "\n", driverName, functionName, __VA_ARGS__);

typedef enum ADXSPD_LOG_LEVEL {
    ADXSPD_LOG_LEVEL_NONE = 0,      // No logging
    ADXSPD_LOG_LEVEL_ERROR = 10,    // Error messages only
    ADXSPD_LOG_LEVEL_WARNING = 20,  // Warnings and errors
    ADXSPD_LOG_LEVEL_INFO = 30,     // Info, warnings, and errors
    ADXSPD_LOG_LEVEL_DEBUG = 40     // Debugging information
} ADXSPD_LogLevel_t;

typedef enum ADXSPD_ON_OFF {
    ADXSPD_OFF = 0,
    ADXSPD_ON = 1,
} ADXSPD_OnOff_t;

typedef enum ADXSPD_COMPRESSOR {
    ADXSPD_COMPRESSOR_NONE = 0,
    ADXSPD_COMPRESSOR_ZLIB = 1,
    ADXSPD_COMPRESSOR_BLOSC = 2,
} ADXSPD_Compressor_t;

typedef enum ADXSPD_SHUFFLE_MODE {
    ADXSPD_SHUFFLE_NONE = 0,
    ADXSPD_AUTO_SHUFFLE = 1,
    ADXSPD_SHUFFLE_BIT = 2,
    ADXSPD_SHUFFLE_BYTE = 3,
} ADXSPD_ShuffleMode_t;

typedef enum ADXSPD_TRIG_MODE {
    ADXSPD_TRIG_SOFTWARE = 0,
    ADXSPD_TRIG_EXT_FRAMES = 1,
    ADXSPD_TRIG_EXT_SEQ = 2,
} ADXSPD_TrigMode_t;


map<ADXSPD_TrigMode_t, string> ADXSPD_TRIG_MODE_MAP = {
    {ADXSPD_TRIG_SOFTWARE, "SOFTWARE"},
    {ADXSPD_TRIG_EXT_FRAMES, "EXT_FRAMES"},
    {ADXSPD_TRIG_EXT_SEQ, "EXT_SEQ"},
};

map<ADXSPD_OnOff_t, string> ADXSPD_ON_OFF_MAP = {
    {ADXSPD_ON, "ON"},
    {ADXSPD_OFF, "OFF"},
};

map<ADXSPD_Compressor_t, string> ADXSPD_COMPRESSOR_MAP = {
    {ADXSPD_COMPRESSOR_NONE, "none"},
    {ADXSPD_COMPRESSOR_ZLIB, "zlib"},
    {ADXSPD_COMPRESSOR_BLOSC, "bslz4"},
};

map<ADXSPD_ShuffleMode_t, string> ADXSPD_SHUFFLE_MODE_MAP = {
    {ADXSPD_SHUFFLE_NONE, "NONE"},
    {ADXSPD_AUTO_SHUFFLE, "AUTO"},
    {ADXSPD_SHUFFLE_BIT, "BIT"},
    {ADXSPD_SHUFFLE_BYTE, "BYTE"},
};

class ADXSPDModule;  // Forward declaration of module class

/*
 * Class definition of the ADXSPD driver
 */
class ADXSPD : ADDriver {
   public:
    // Constructor for the ADXSPD driver
    ADXSPD(const char* portName, const char* ipPort, const char* deviceId);

    // ADDriver overrides
    virtual asynStatus writeInt32(asynUser* pasynUser, epicsInt32 value);
    virtual asynStatus writeFloat64(asynUser* pasynUser, epicsFloat64 value);
    virtual void report(FILE* fp, int details);

    // Destructor. Disconnects from the detector and performs cleanup
    ~ADXSPD();

    // Must be public, since it is called from an external C function
    void acquisitionThread();
    void monitorThread();

    ADXSPD_LogLevel_t getLogLevel() { return this->logLevel; }
    string getDeviceId() { return this->deviceId; }
    string getDetectorId() { return this->detectorId; }

    json xspdGet(string uri);

    template <typename T>
    T xspdGetVar(string endpoint, string key = "value");

    template <typename T>
    asynStatus xspdSet(string endpoint, T value);

    asynStatus xspdCommand(string command);

    ADXSPD_OnOff_t parseOnOff(string value);
    ADXSPD_ShuffleMode_t parseShuffleMode(string value);

   protected:
// Load auto-generated parameter string and index definitions
#include "ADXSPDParamDefs.h"

   private:
    const char* driverName = "ADXSPD";
    void createAllParams();

    void getInitialState();

    bool alive = true;  // Flag to indicate whether our acquisition thread and monitor thread should
                        // keep running
    epicsThreadId acquisitionThreadId;
    epicsThreadId monitorThreadId;

    void* zmqContext;
    void* zmqSubscriber;

    void acquireStart();
    void acquireStop();
    vector<ADXSPDModule*> modules;

    void getDetectorConfiguration();
    void checkOnOffVariable(string endpoint, int paramIndex);

    string apiUri;      // IP address and port for the device
    string deviceUri;   // Base URI for the device
    string deviceVarUri; // Base URI for device variables
    string deviceId;    // Device ID for the XSPD device
    string detectorId;  // Detector ID
    string dataPortId;
    string dataPortIp;
    int dataPortPort;

    ADXSPD_LogLevel_t logLevel = ADXSPD_LOG_LEVEL_DEBUG;  // Logging level for the driver
};

#endif
