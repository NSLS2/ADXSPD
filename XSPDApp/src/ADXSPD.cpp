/**
 * Main source file for the ADXSPD EPICS driver
 *
 * This file was initially generated with the help of the ADDriverTemplate:
 * https://github.com/NSLS2/ADDriverTemplate on 01/07/2025
 *
 * Author: Jakub Wlodek
 *
 * Copyright (c) : Brookhaven National Laboratory, 2025
 *
 */

#include "ADXSPD.h"

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

using namespace std;

const char* driverName = "ADXSPD";

/*
 * External configuration function for ADXSPD.
 * Envokes the constructor to create a new ADXSPD object
 * This is the function that initializes the driver, and is called in the IOC startup script
 *
 * @params[in]: all passed into constructor
 * @return:     status
 */
extern "C" int ADXSPDConfig(const char* portName, .....) {
    new ADXSPD(portName, ......);
    return (asynSuccess);
}

/*
 * Callback function fired when IOC is terminated.
 * Deletes the driver object and performs cleanup
 *
 * @params[in]: pPvt -> pointer to the driver object created in ADXSPDConfig
 * @return:     void
 */
static void exitCallbackC(void* pPvt) {
    ADXSPD* pXSPD = (ADXSPD*)pPvt;
    delete (pXSPD);
}

/**
 * @brief Wrapper C function passed to epicsThreadCreate to create acquisition thread
 *
 * @param drvPvt Pointer to instance of ADXSPD driver object
 */
static void acquisitionThreadC(void* drvPvt) {
    ADXSPD* pPvt = (ADXSPD*)drvPvt;
    pPvt->acquisitionThread();
}

// -----------------------------------------------------------------------
// ADXSPD Acquisition Functions
// -----------------------------------------------------------------------

/**
 * Function that spawns new acquisition thread, if able
 */
void ADXSPD::acquireStart() {
    const char* functionName = "acquireStart";

    // Spawn the acquisition thread. Make sure it's joinable.
    INFO("Spawning main acquisition thread...");

    epicsThreadOpts opts;
    opts.priority = epicsThreadPriorityHigh;
    opts.stackSize = epicsThreadGetStackSize(epicsThreadStackBig);
    opts.joinable = 1;

    this->acquisitionThreadId =
        epicsThreadCreateOpt("acquisitionThread", (EPICSTHREADFUNC)acquisitionThreadC, this, &opts);
}

/**
 * Main acquisition function for ADXSPD
 */
void ADXSPD::acquisitionThread() {
    const char* functionName = "acquisitionThread";

    NDDataType_t dataType;
    NDColorMode_t colorMode;
    getIntegerParam(NDColorMode, (int*)&colorMode);
    getIntegerParam(NDDataType, (int*)&dataType);

    int ndims = 3;                            // For color
    if (colorMode == NDColorMono) ndims = 2;  // For monochrome

    int dims[ndims];
    if (ndims == 2) {
        getIntegerParam(ADSizeX, &dims[0]);
        getIntegerParam(ADSizeY, &dims[1]);
    } else {
        dims[0] = 3;
        getIntegerParam(ADSizeX, &dims[1]);
        getIntegerParam(ADSizeY, &dims[2]);
    }

    int collectedImages = 0;

    // Start the acquisition given resolution, data type, color mode here
    this->acquisitionActive = true;

    while (acquisitionActive) {
        setIntegerParam(ADStatus, ADStatusAcquire);

        // Get a new frame using the vendor SDK here here

        // Allocate the NDArray of the correct size
        this->pArrays[0] = pNDArrayPool->alloc(ndims, dims, dataType, 0, NULL);
        if (this->pArrays[0] != NULL) {
            pArray = this->pArrays[0];
        } else {
            this->pArrays[0]->release();
            ERR("Failed to allocate array!");
            setIntegerParam(ADStatus, ADStatusError);
            callParamCallbacks();
            break;
        }

        collectedImages += 1;
        setIntegerParam(ADNumImagesCounter, collectedImages);
        updateTimeStamp(&pArray->epicsTS);

        // Set array size PVs based on collected frame
        pArray->getInfo(&arrayInfo);
        setIntegerParam(NDArraySize, (int)arrayInfo.totalBytes);
        setIntegerParam(NDArraySizeX, arrayInfo.xSize);
        setIntegerParam(NDArraySizeY, arrayInfo.ySize);

        // Copy data from new frame to pArray
        memcpy(pArray->pData, POINTER_TO_FRAME_DATA, arrayInfo.totalBytes);

        // increment the array counter
        int arrayCounter;
        getIntegerParam(NDArrayCounter, &arrayCounter);
        arrayCounter++;
        setIntegerParam(NDArrayCounter, arrayCounter);

        // set the image unique ID to the number in the sequence
        pArray->uniqueId = arrayCounter;
        pArray->pAttributeList->add("ColorMode", "Color Mode", NDAttrInt32, &colorMode);

        getAttributes(pArray->pAttributeList);
        doCallbacksGenericPointer(pArray, NDArrayData, 0);

        // If in single mode, finish acq, if in multiple mode and reached target number
        // complete acquisition.
        if (acquisitionMode == ADImageSingle) {
            this->acquisitionActive = false;
        } else if (acquisitionMode == ADImageMultiple && collectedImages == targetNumImages) {
            this->acquisitionActive = false;
        }
        // Release the array
        pArray->release();

        // refresh all PVs
        callParamCallbacks();
    }

    setIntegerParam(ADStatus, ADStatusIdle);
    setIntegerParam(ADAcquire, 0);
    callParamCallbacks();
}

/**
 * @brief stops acquisition by aborting exposure and joinging acq thread
 */
void ADXSPD::acquireStop() {
    const char* functionName = "acquireStop";

    if (this->acquisitionActive) {
        // Mark acquisition as inactive
        this->acquisitionActive = false;

        // Wait for acquisition thread to join
        INFO("Waiting for acquisition thread to join...");
        epicsThreadMustJoin(this->acquisitionThreadId);
        INFO("Acquisition stopped.");

        // Refresh all PV values
        callParamCallbacks();
    } else {
        WARN("Acquisition is not active!");
    }
}

//-------------------------------------------------------------------------
// ADDriver function overwrites
//-------------------------------------------------------------------------

/*
 * Function overwriting ADDriver base function.
 * Takes in a function (PV) changes, and a value it is changing to, and processes the input
 *
 * @params[in]: pasynUser       -> asyn client who requests a write
 * @params[in]: value           -> int32 value to write
 * @return: asynStatus      -> success if write was successful, else failure
 */
asynStatus ADXSPD::writeInt32(asynUser* pasynUser, epicsInt32 value) {
    int function = pasynUser->reason;
    int acquiring;
    int status = asynSuccess;
    static const char* functionName = "writeInt32";
    getIntegerParam(ADAcquire, &acquiring);

    status = setIntegerParam(function, value);
    // start/stop acquisition
    if (function == ADAcquire) {
        if (value && !acquiring) {
            acquireStart();
        }
        if (!value && acquiring) {
            acquireStop();
        }
    }

    else if (function == ADImageMode)
        if (acquiring == 1)
            acquireStop();

        else {
            if (function < ADXSPD_FIRST_PARAM) {
                status = ADDriver::writeInt32(pasynUser, value);
            }
        }
    callParamCallbacks();

    if (status) {
        ERR_ARGS("status=%d, function=%d, value=%d\n", status, function, value);
        return asynError;
    } else
        DEBUG_ARGS("function=%d value=%d\n", function, value);
    return asynSuccess;
}

/*
 * Function overwriting ADDriver base function.
 * Takes in a function (PV) changes, and a value it is changing to, and processes the input
 * This is the same functionality as writeInt32, but for processing doubles.
 *
 * @params[in]: pasynUser       -> asyn client who requests a write
 * @params[in]: value           -> int32 value to write
 * @return: asynStatus      -> success if write was successful, else failure
 */
asynStatus ADXSPD::writeFloat64(asynUser* pasynUser, epicsFloat64 value) {
    int function = pasynUser->reason;
    int acquiring;
    asynStatus status = asynSuccess;
    static const char* functionName = "writeFloat64";
    getIntegerParam(ADAcquire, &acquiring);

    status = setDoubleParam(function, value);

    if (function == ADAcquireTime) {
        if (acquiring) acquireStop();
    } else {
        if (function < ADXSPD_FIRST_PARAM) {
            status = ADDriver::writeFloat64(pasynUser, value);
        }
    }
    callParamCallbacks();

    if (status) {
        ERR_ARGS("status=%d, function=%d, value=%f\n", status, function, value);
        return asynError;
    } else
        DEBUG_ARGS("function=%d value=%f\n", function, value);
    return status;
}

/*
 * Function used for reporting ADUVC device and library information to a external
 * log file. The function first prints all libuvc specific information to the file,
 * then continues on to the base ADDriver 'report' function
 *
 * @params[in]: fp      -> pointer to log file
 * @params[in]: details -> number of details to write to the file
 * @return: void
 */
void ADXSPD::report(FILE* fp, int details) {
    const char* functionName = "report";
    if (details > 0) {
        ADDriver::report(fp, details);
    }
}

//----------------------------------------------------------------------------
// ADXSPD Constructor/Destructor
//----------------------------------------------------------------------------

ADXSPD::ADXSPD(const char* portName, .....)
    : ADDriver(portName, 1, (int)NUM_XSPD_PARAMS, 0, 0, 0, 0, 0, 1, 0, 0) {
    static const char* functionName = "ADXSPD";

    // Call createParam here for all of your
    // ex. createParam(ADUVC_UVCComplianceLevelString, asynParamInt32, &ADUVC_UVCComplianceLevel);

    // Sets driver version PV (version numbers defined in header file)
    char versionString[25];
    epicsSnprintf(versionString, sizeof(versionString), "%d.%d.%d", ADXSPD_VERSION, ADXSPD_REVISION,
                  ADXSPD_MODIFICATION);
    setStringParam(NDDriverVersion, versionString);

    // Initialzie vendor SDK and connect to the device here

    // Retrieve device information and populate all PVs.
    setStringParam(ADManufacturer, ....);
    setStringParam(ADModel, ....);
    setStringParam(ADSerialNumber, ....);
    setStringParam(ADFirmwareVersion, ....);
    setStringParam(ADSDKVersion, ....);
    setIntegerParam(ADMaxSizeX, ....);
    setIntegerParam(ADMaxSizeY, ....);

    setIntegerParam(ADMinX, 0);
    setIntegerParam(ADMinY, 0);

    callParamCallbacks();

    // when epics is exited, delete the instance of this class
    epicsAtExit(exitCallbackC, this);
}

ADXSPD::~ADXSPD() {
    const char* functionName = "~ADXSPD";

    INFO("Shutting down ADXSPD driver...");
    if (this->acquisitionActive && this->acquisitionThreadId != NULL) acquireStop();

    // Destroy any resources allocated by the vendor SDK here
    INFO("Done.");
}

//-------------------------------------------------------------
// ADXSPD ioc shell registration
//-------------------------------------------------------------

/* XSPDConfig -> These are the args passed to the constructor in the epics config function */
static const iocshArg XSPDConfigArg0 = {"Port name", iocshArgString};

// This parameter must be customized by the driver author. Generally a URL, Serial Number, ID, IP
// are used to connect.
static const iocshArg XSPDConfigArg1 = {"Connection Param", iocshArgString};

/* Array of config args */
static const iocshArg* const XSPDConfigArgs[] = {&XSPDConfigArg0, &XSPDConfigArg1};

/* what function to call at config */
static void configXSPDCallFunc(const iocshArgBuf* args) {
    ADXSPDConfig(args[0].sval, args[1].sval);
}

/* information about the configuration function */
static const iocshFuncDef configUVC = {"ADXSPDConfig", 2, XSPDConfigArgs};

/* IOC register function */
static void XSPDRegister(void) { iocshRegister(&configXSPD, configXSPDCallFunc); }

/* external function for IOC register */
extern "C" {
epicsExportRegistrar(XSPDRegister);
}
