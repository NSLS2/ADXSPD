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
#include <magic_enum/magic_enum.hpp>
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
#include <type_traits>
#include <map>
#include <nlohmann/json.hpp>
#include <string>

#include "ADDriver.h"
#include "epicsThread.h"

using json = nlohmann::json;
using namespace std;

// Error message formatters
#define ERR(msg)                                      \
    if (this->getLogLevel() >= ADXSPDLogLevel::ERROR) \
        printf("ERROR | %s::%s: %s\n", driverName, functionName, msg);

#define ERR_ARGS(fmt, ...)                            \
    if (this->getLogLevel() >= ADXSPDLogLevel::ERROR) \
        printf("ERROR | %s::%s: " fmt "\n", driverName, functionName, __VA_ARGS__);

// Warning message formatters
#define WARN(msg)                                       \
    if (this->getLogLevel() >= ADXSPDLogLevel::WARNING) \
        printf("WARNING | %s::%s: %s\n", driverName, functionName, msg);

#define WARN_ARGS(fmt, ...)                             \
    if (this->getLogLevel() >= ADXSPDLogLevel::WARNING) \
        printf("WARNING | %s::%s: " fmt "\n", driverName, functionName, __VA_ARGS__);

// Info message formatters. Because there is no ASYN trace for info messages, we just use `printf`
// here.
#define INFO(msg)                                    \
    if (this->getLogLevel() >= ADXSPDLogLevel::INFO) \
        printf("INFO | %s::%s: %s\n", driverName, functionName, msg);

#define INFO_ARGS(fmt, ...)                          \
    if (this->getLogLevel() >= ADXSPDLogLevel::INFO) \
        printf("INFO | %s::%s: " fmt "\n", driverName, functionName, __VA_ARGS__);

// Debug message formatters
#define DEBUG(msg)                                    \
    if (this->getLogLevel() >= ADXSPDLogLevel::DEBUG) \
        printf("DEBUG | %s::%s: %s\n", driverName, functionName, msg);

#define DEBUG_ARGS(fmt, ...)                          \
    if (this->getLogLevel() >= ADXSPDLogLevel::DEBUG) \
        printf("DEBUG | %s::%s: " fmt "\n", driverName, functionName, __VA_ARGS__);

enum class ADXSPDLogLevel {
    NONE = 0,      // No logging
    ERROR = 10,    // Error messages only
    WARNING = 20,  // Warnings and errors
    INFO = 30,     // Info, warnings, and errors
    DEBUG = 40     // Debugging information
};

enum class ADXSPDOnOff {
    OFF = 0,
    ON = 1,
};

enum class ADXSPDCompressor {
    NONE = 0,
    ZLIB = 1,
    BLOSC = 2,
};

enum class ADXSPDShuffleMode {
    NONE = 0,
    AUTO_SHUFFLE = 1,
    SHUFFLE_BIT = 2,
    SHUFFLE_BYTE = 3,
};

enum class ADXSPDTrigMode {
    SOFTWARE = 0,
    EXT_FRAMES = 1,
    EXT_SEQ = 2,
};

enum class ADXSPDCounterMode {
    SINGLE = 0,
    DUAL = 1,
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

    ADXSPDLogLevel getLogLevel() { return this->logLevel; }
    string getDeviceId() { return this->deviceId; }
    string getDetectorId() { return this->detectorId; }

    json xspdGet(string uri);

    template <typename T>
    T xspdGetVar(string endpoint, string key = "value");

    template <typename T>
    T xspdGetDetVar(string endpoint, string key = "value");

    template <typename T>
    T xspdGetDataPortVar(string endpoint, string key = "value");

    template <typename T>
    T xspdGetModuleVar(int moduleIndex, string endpoint, string key = "value");

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

    void getInitialDetState();
    void checkOnOffVariable(string endpoint, int paramIndex);

    string apiUri;      // IP address and port for the device
    string deviceUri;   // Base URI for the device
    string deviceVarUri; // Base URI for device variables
    string deviceId;    // Device ID for the XSPD device
    string detectorId;  // Detector ID
    string dataPortId;
    string dataPortIp;
    int dataPortPort;

    ADXSPDLogLevel logLevel = ADXSPDLogLevel::DEBUG;  // Logging level for the driver
};

#endif
