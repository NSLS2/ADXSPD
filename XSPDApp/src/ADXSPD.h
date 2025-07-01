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

// Error message formatters
#define ERR(msg)                                                                                 \
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "ERROR | %s::%s: %s\n", driverName, functionName, \
              msg)

#define ERR_ARGS(fmt, ...)                                                              \
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "ERROR | %s::%s: " fmt "\n", driverName, \
              functionName, __VA_ARGS__);

// Warning message formatters
#define WARN(msg)                                                                      \
    asynPrint(pasynUserSelf, ASYN_TRACE_WARNING, "WARNING | %s::%s: %s\n", driverName, \
              functionName, msg)

#define WARN_ARGS(fmt, ...)                                                                 \
    asynPrint(pasynUserSelf, ASYN_TRACE_WARNING, "WARNING | %s::%s: " fmt "\n", driverName, \
              functionName, __VA_ARGS__);

// Info message formatters
#define INFO(msg) \
    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, "INFO | %s::%s: %s\n", driverName, functionName, msg)

#define INFO_ARGS(fmt, ...)                                                           \
    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, "INFO | %s::%s: " fmt "\n", driverName, \
              functionName, __VA_ARGS__);

// Debug message formatters
#define DEBUG(msg) \
    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, "DEBUG | %s::%s: %s\n", driverName, functionName, msg)

#define DEBUG_ARGS(fmt, ...)                                                           \
    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, "DEBUG | %s::%s: " fmt "\n", driverName, \
              functionName, __VA_ARGS__);

// Define macros that correspond to string representations of PVs from EPICS database template here.
// For example: For a record with `OUT` field set to `@asyn($(PORT), $(ADDR=0),
// $(TIMEOUT=1))PV_NAME`, add, where the record represents an integer value: #define
// ADXSPD_PVNameString          "PV_NAME"            //asynInt32

#include "ADDriver.h"

/*
 * Class definition of the ADXSPD driver
 */
class ADXSPD : ADDriver {
   public:
    // Constructor for the ADXSPD driver
    ADXSPD(const char* portName, ......);

    // ADDriver overrides
    virtual asynStatus writeInt32(asynUser* pasynUser, epicsInt32 value);
    virtual asynStatus writeFloat64(asynUser* pasynUser, epicsFloat64 value);

    // Destructor. Disconnects from the detector and performs cleanup
    ~ADXSPD();

   protected:
// Add PV indexes here. You must also define the first/last index as you add them.
// Ex: int ADXSPD_PVName;
#define ADXSPD_FIRST_PARAM ......
#define ADXSPD_LAST_PARAM ......

   private:
    bool acquisitionActive;  // Flag to indicate if acquisition is active
    epicsThreadId acquisitionThreadId;

    void acquireStart();
    void acquireStop();
    void acquisitionThread();
};

// Stores number of additional PV parameters are added by the driver
#define NUM_XSPD_PARAMS ((int)(&ADXSPD_LAST_PARAM - &ADXSPD_FIRST_PARAM + 1))

#endif
