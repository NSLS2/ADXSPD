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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zmq.h>

#include <nlohmann/json.hpp>

#include "ADDriver.h"
#include "ADXSPDModule.h"

using json = nlohmann::json;  // For JSON handling
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

class ADXSPDModule;  // Forward declaration

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

    template <typename T>
    T xspdGet(string endpoint, string key);

    template <typename T>
    asynStatus xspdSet(string endpoint, T value);

    asynStatus xspdCommand(string command);

   protected:
// Load auto-generated parameter string and index definitions
#include "ADXSPDParamDefs.h"

   private:
    const char* driverName = "ADXSPD";
    void createAllParams();

    bool alive = true;  // Flag to indicate whether our acquisition thread and monitor thread should
                        // keep running
    epicsThreadId acquisitionThreadId;
    epicsThreadId monitorThreadId;

    void* zmqContext;
    void* zmqSubscriber;

    void acquireStart();
    void acquireStop();
    vector<ADXSPDModule*> modules;

    string apiUri;      // IP address and port for the device
    string deviceId;    // Device ID for the XSPD device
    string detectorId;  // Detector ID
    string dataPortId;
    string dataPortIp;
    int dataPortPort;

    ADXSPD_LogLevel_t logLevel = ADXSPD_LOG_LEVEL_INFO;  // Logging level for the driver
};

#endif
