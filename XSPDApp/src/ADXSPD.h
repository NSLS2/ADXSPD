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
#define ADXSPD_MODIFICATION 0

#include <cpr/cpr.h>

#include <json.hpp>

#include "ADDriver.h"

#define ADXSPD_NumModulesString "XSPD_NUM_MODULES"
#define ADXSPD_APIVersionString "XSPD_API_VERSION"
#define ADXSPD_XSPDVersionString "XSPD_VERSION"

using json = nlohmann::json;  // For JSON handling
using namespace std;

typedef enum ADXSPD_LOG_LEVEL {
    ADXSPD_LOG_LEVEL_NONE = 0,      // No logging
    ADXSPD_LOG_LEVEL_ERROR = 10,    // Error messages only
    ADXSPD_LOG_LEVEL_WARNING = 20,  // Warnings and errors
    ADXSPD_LOG_LEVEL_INFO = 30,     // Info, warnings, and errors
    ADXSPD_LOG_LEVEL_DEBUG = 40     // Debugging information
} ADXSPD_LogLevel_t;

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

   protected:
    int ADXSPD_NumModules;
    int ADXSPD_APIVersion;
    int ADXSPD_XSPDVersion;

#define ADXSPD_FIRST_PARAM ADXSPD_NumModules
#define ADXSPD_LAST_PARAM ADXSPD_XSPDVersion

   private:
    bool acquisitionActive;  // Flag to indicate if acquisition is active
    epicsThreadId acquisitionThreadId;

    void acquireStart();
    void acquireStop();

    json xspdGet(string endpoint);
    json xspdGetValue(string path);

    string apiUri;    // IP address and port for the device
    string deviceId;  // Device ID for the XSPD device

    ADXSPD_LogLevel_t logLevel = ADXSPD_LOG_LEVEL_INFO;  // Logging level for the driver
};

// Stores number of additional PV parameters are added by the driver
#define NUM_XSPD_PARAMS ((int)(&ADXSPD_LAST_PARAM - &ADXSPD_FIRST_PARAM + 1))

#endif
