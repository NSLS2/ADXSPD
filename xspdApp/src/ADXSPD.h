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
#define ADXSPD_REVISION 1
#define ADXSPD_MODIFICATION 0

#include <blosc.h>
#include <cpr/cpr.h>
#include <epicsExit.h>
#include <epicsExport.h>
#include <epicsStdio.h>
#include <epicsString.h>
#include <epicsThread.h>
#include <epicsTime.h>
#include <iocsh.h>
#include <stdlib.h>
#include <zlib.h>
#include <zmq.h>

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <magic_enum/magic_enum.hpp>
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <type_traits>

#include "ADDriver.h"
#include "XSPDAPI.h"

using json = nlohmann::json;
using namespace std;

// Error message formatters
#define ERR(msg)                                      \
    if (this->getLogLevel() >= ADXSPDLogLevel::ERROR) \
        fprintf(stderr, "ERROR | %s::%s: %s\n", driverName, __func__, msg);

#define ERR_ARGS(fmt, ...)                            \
    if (this->getLogLevel() >= ADXSPDLogLevel::ERROR) \
        fprintf(stderr, "ERROR | %s::%s: " fmt "\n", driverName, __func__, __VA_ARGS__);

#define ERR_TO_STATUS(fmt, ...)                                       \
    if (this->getLogLevel() >= ADXSPDLogLevel::ERROR) {               \
        char errMsg[256];                                             \
        snprintf(errMsg, sizeof(errMsg), fmt, __VA_ARGS__);           \
        printf("ERROR | %s::%s: %s\n", driverName, __func__, errMsg); \
        setStringParam(ADStatusMessage, errMsg);                      \
        setIntegerParam(ADStatus, ADStatusError);                     \
        callParamCallbacks();                                         \
    }

// Warning message formatters
#define WARN(msg)                                       \
    if (this->getLogLevel() >= ADXSPDLogLevel::WARNING) \
        fprintf(stderr, "WARNING | %s::%s: %s\n", driverName, __func__, msg);

#define WARN_ARGS(fmt, ...)                             \
    if (this->getLogLevel() >= ADXSPDLogLevel::WARNING) \
        fprintf(stderr, "WARNING | %s::%s: " fmt "\n", driverName, __func__, __VA_ARGS__);

#define WARN_TO_STATUS(fmt, ...)                                         \
    if (this->getLogLevel() >= ADXSPDLogLevel::WARNING) {                \
        char warnMsg[256];                                               \
        snprintf(warnMsg, sizeof(warnMsg), fmt, __VA_ARGS__);            \
        printf("WARNING | %s::%s: %s\n", driverName, __func__, warnMsg); \
        setStringParam(ADStatusMessage, warnMsg);                        \
        callParamCallbacks();                                            \
    }

// Info message formatters
#define INFO(msg)                                    \
    if (this->getLogLevel() >= ADXSPDLogLevel::INFO) \
        fprintf(stdout, "INFO | %s::%s: %s\n", driverName, __func__, msg);

#define INFO_ARGS(fmt, ...)                          \
    if (this->getLogLevel() >= ADXSPDLogLevel::INFO) \
        fprintf(stdout, "INFO | %s::%s: " fmt "\n", driverName, __func__, __VA_ARGS__);

#define INFO_TO_STATUS(fmt, ...)                                      \
    if (this->getLogLevel() >= ADXSPDLogLevel::INFO) {                \
        char infoMsg[256];                                            \
        snprintf(infoMsg, sizeof(infoMsg), fmt, __VA_ARGS__);         \
        printf("INFO | %s::%s: %s\n", driverName, __func__, infoMsg); \
        setStringParam(ADStatusMessage, infoMsg);                     \
        callParamCallbacks();                                         \
    }

// Debug message formatters
#define DEBUG(msg)                                    \
    if (this->getLogLevel() >= ADXSPDLogLevel::DEBUG) \
        fprintf(stdout, "DEBUG | %s::%s: %s\n", driverName, __func__, msg);

#define DEBUG_ARGS(fmt, ...)                          \
    if (this->getLogLevel() >= ADXSPDLogLevel::DEBUG) \
        fprintf(stdout, "DEBUG | %s::%s: " fmt "\n", driverName, __func__, __VA_ARGS__);

enum class ADXSPDLogLevel {
    NONE = 0,      // No logging
    ERROR = 10,    // Error messages only
    WARNING = 20,  // Warnings and errors
    INFO = 30,     // Info, warnings, and errors
    DEBUG = 40     // Debugging information
};

#define ADXSPD_MIN_STATUS_POLL_INTERVAL 0.5  // Minimum status poll interval in seconds

class ADXSPDModule;  // Forward declaration of module class

/*
 * Class definition of the ADXSPD driver
 */
class ADXSPD : ADDriver {
   public:
    // Constructor for the ADXSPD driver
    ADXSPD(const char* portName, const char* ip, int portNum, const char* deviceId = nullptr);

    // ADDriver overrides
    virtual asynStatus writeInt32(asynUser* pasynUser, epicsInt32 value);
    virtual asynStatus writeFloat64(asynUser* pasynUser, epicsFloat64 value);
    virtual void report(FILE* fp, int details);

    // Destructor. Disconnects from the detector and performs cleanup
    ~ADXSPD();

    // Must be public, since it is called from an external C function
    void acquisitionThread();
    void monitorThread();

    ADXSPDLogLevel getLogLevel() { return this->logLevel; }

    void getInitialDetState();
    asynStatus acquireStart();
    asynStatus acquireStop();
    NDDataType_t getDataTypeForBitDepth(int bitDepth);

    template <typename T>
    void subtractFrames(void* currentFrame, void* previousFrame, void* outputFrame,
                        size_t numBytes);

   protected:
// Load auto-generated parameter string and index definitions
#include "ADXSPDParamDefs.h"

   private:
    const char* driverName = "ADXSPD";
    void createAllParams();

    epicsThreadId acquisitionThreadId;
    epicsThreadId monitorThreadId;

    epicsEventId shutdownEventId;

    void* zmqContext;

    vector<ADXSPDModule*> modules;
    XSPD::API* pApi;
    XSPD::Detector* pDetector;

    string apiUri;        // IP address and port for the device
    string deviceUri;     // Base URI for the device
    string deviceVarUri;  // Base URI for device variables
    string deviceId;      // Device ID for the XSPD device
    string detectorId;    // Detector ID
    string dataPortId;
    string dataPortIp;
    int dataPortPort;

    vector<int> onlyIdleParams = {
        ADTriggerMode, ADAcquireTime, ADXSPD_BitDepth, ADXSPD_ShuffleMode, ADXSPD_CounterMode,
    };

    ADXSPDLogLevel logLevel = ADXSPDLogLevel::DEBUG;  // Logging level for the driver
};

#endif
