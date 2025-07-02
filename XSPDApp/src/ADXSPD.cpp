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

using json = nlohmann::json;
using namespace std;

const char* driverName = "ADXSPD";

// Error message formatters
#define ERR(msg)                                  \
    if (this->logLevel >= ADXSPD_LOG_LEVEL_ERROR) \
        printf("ERROR | %s::%s: %s\n", driverName, functionName, msg);

#define ERR_ARGS(fmt, ...)                        \
    if (this->logLevel >= ADXSPD_LOG_LEVEL_ERROR) \
        printf("ERROR | %s::%s: " fmt "\n", driverName, functionName, __VA_ARGS__);

// Warning message formatters
#define WARN(msg)                                   \
    if (this->logLevel >= ADXSPD_LOG_LEVEL_WARNING) \
        printf("WARNING | %s::%s: %s\n", driverName, functionName, msg);

#define WARN_ARGS(fmt, ...)                         \
    if (this->logLevel >= ADXSPD_LOG_LEVEL_WARNING) \
        printf("WARNING | %s::%s: " fmt "\n", driverName, functionName, __VA_ARGS__);

// Info message formatters. Because there is no ASYN trace for info messages, we just use `printf`
// here.
#define INFO(msg)                                \
    if (this->logLevel >= ADXSPD_LOG_LEVEL_INFO) \
        printf("INFO | %s::%s: %s\n", driverName, functionName, msg);

#define INFO_ARGS(fmt, ...)                      \
    if (this->logLevel >= ADXSPD_LOG_LEVEL_INFO) \
        printf("INFO | %s::%s: " fmt "\n", driverName, functionName, __VA_ARGS__);

// Debug message formatters
#define DEBUG(msg)                                \
    if (this->logLevel >= ADXSPD_LOG_LEVEL_DEBUG) \
    printf("DEBUG | %s::%s: %s\n", driverName, functionName, msg)

#define DEBUG_ARGS(fmt, ...)                      \
    if (this->logLevel >= ADXSPD_LOG_LEVEL_DEBUG) \
        printf("DEBUG | %s::%s: " fmt "\n", driverName, functionName, __VA_ARGS__);

/*
 * External configuration function for ADXSPD.
 * Envokes the constructor to create a new ADXSPD object
 * This is the function that initializes the driver, and is called in the IOC startup script
 *
 * @params[in]: all passed into constructor
 * @return:     status
 */
extern "C" int ADXSPDConfig(const char* portName, const char* ip, int ctrlPort,
                            const char* deviceId) {
    new ADXSPD(portName, ip, ctrlPort, deviceId);
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

static string capitalize(const string& str) {
    if (str.empty()) return str;
    string result = str;
    result[0] = toupper(result[0]);
    return result;
}

json ADXSPD::xspdGet(string endpoint) {
    const char* functionName = "xspdGet";
    // Make a GET request to the XSPD API
    cpr::Response response = cpr::Get(cpr::Url(this->apiUri + "/" + endpoint));

    if (response.status_code != 200) {
        ERR_ARGS("Failed to get data from %s: %s", endpoint.c_str(),
                 response.error.message.c_str());
        return json();
    }

    try {
        return json::parse(response.text.c_str());
    } catch (json::parse_error& e) {
        ERR_ARGS("Failed to parse JSON response from %s: %s", endpoint.c_str(), e.what());
        return json();
    }
}

json ADXSPD::xspdGetValue(string path) {
    const char* functionName = "xspdGetValue";
    json response = xspdGet("v1/devices/" + this->deviceId + "/variables?path=" + path);
    if (response.empty()) {
        ERR_ARGS("Failed to get value for path %s", path.c_str());
        return json();
    }
    return response["value"];
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

    NDArray* pArray = NULL;
    ADImageMode_t acquisitionMode;
    NDArrayInfo arrayInfo;
    NDDataType_t dataType;
    NDColorMode_t colorMode = NDColorModeMono;  // Only monochrome is supported.

    getIntegerParam(NDDataType, (int*)&dataType);
    getIntegerParam(ADImageMode, (int*)&acquisitionMode);

    size_t dims[2];
    getIntegerParam(ADSizeX, (int*)&dims[0]);
    getIntegerParam(ADSizeY, (int*)&dims[1]);

    int collectedImages = 0;
    int targetNumImages;
    getIntegerParam(ADNumImages, &targetNumImages);

    // Start the acquisition given resolution, data type, color mode here
    this->acquisitionActive = true;

    while (acquisitionActive) {
        setIntegerParam(ADStatus, ADStatusAcquire);

        // Get a new frame using the vendor SDK here here

        // Allocate the NDArray of the correct size
        this->pArrays[0] = pNDArrayPool->alloc(2, dims, dataType, 0, NULL);
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
        // memcpy(pArray->pData, POINTER_TO_FRAME_DATA, arrayInfo.totalBytes);

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
        } else if (!value && acquiring) {
            acquireStop();
        }
    } else if (function == ADImageMode) {
        if (acquiring == 1) {
            acquireStop();
        } else if (function < ADXSPD_FIRST_PARAM) {
            status = ADDriver::writeInt32(pasynUser, value);
        }
    }
    callParamCallbacks();

    if (status) {
        ERR_ARGS("status=%d, function=%d, value=%d", status, function, value);
        return asynError;
    } else {
        DEBUG_ARGS("function=%d value=%d", function, value);
        return asynSuccess;
    }
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
        ERR_ARGS("status=%d, function=%d, value=%f", status, function, value);
        return asynError;
    } else {
        DEBUG_ARGS("function=%d value=%f", function, value);
        return status;
    }
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

ADXSPD::ADXSPD(const char* portName, const char* ip, int ctrlPort, const char* deviceId)
    : ADDriver(portName, 1, (int)NUM_XSPD_PARAMS, 0, 0, 0, 0, 0, 1, 0, 0) {
    static const char* functionName = "ADXSPD";

    createParam(ADXSPD_NumModulesString, asynParamInt32, &ADXSPD_NumModules);
    createParam(ADXSPD_APIVersionString, asynParamOctet, &ADXSPD_APIVersion);
    createParam(ADXSPD_XSPDVersionString, asynParamOctet, &ADXSPD_XSPDVersion);

    // Sets driver version PV (version numbers defined in header file)
    char versionString[25];
    epicsSnprintf(versionString, sizeof(versionString), "%d.%d.%d", ADXSPD_VERSION, ADXSPD_REVISION,
                  ADXSPD_MODIFICATION);
    setStringParam(NDDriverVersion, versionString);

    // Initialize vendor SDK and connect to the device here
    this->apiUri = string(ip) + ":" + to_string(ctrlPort) + "/api";
    INFO_ARGS("Connecting to XSPD api at %s", this->apiUri.c_str());

    json xspdVersionInfo = xspdGet("");
    if (xspdVersionInfo.empty()) {
        ERR("Failed to retrieve XSPD version information.");
        return;
    }
    setStringParam(ADXSPD_APIVersion, xspdVersionInfo["api version"].get<string>().c_str());
    setStringParam(ADXSPD_XSPDVersion, xspdVersionInfo["xspd version"].get<string>().c_str());

    json deviceList = xspdGet("v1/devices")["devices"];
    if (deviceList.empty()) {
        ERR("No devices found!");
        return;
    }

    json requestedDevice = NULL;

    INFO_ARGS("Found %ld devices", deviceList.size());
    if (deviceId == NULL) {
        INFO("Device ID not specified, using first device in list.");
        requestedDevice = deviceList[0];

    } else {
        for (const auto& device : deviceList) {
            if (strcmp(device["id"].get<string>().c_str(), deviceId) == 0) {
                requestedDevice = device;
                break;
            }
        }
    }

    if (requestedDevice == NULL) {
        ERR_ARGS("Device with ID %s not found in device list.", deviceId);
        return;
    }

    this->deviceId = requestedDevice["id"].get<string>();
    INFO_ARGS("Retrieving info for device with ID %s...", this->deviceId.c_str());

    json deviceInfo = xspdGetValue("info");
    if (deviceInfo.empty()) {
        ERR_ARGS("Failed to retrieve device info for device ID %s", this->deviceId.c_str());
        return;
    }

    json detectorInfo = deviceInfo["detectors"][0];

    // Retrieve device information and populate all PVs.
    setStringParam(ADManufacturer, "X-Spectrum GmbH");
    setStringParam(ADModel, capitalize(detectorInfo["detector-id"].get<string>()).c_str());
    // setStringParam(ADSerialNumber, ....);
    // setStringParam(ADFirmwareVersion, ....);
    setStringParam(ADSDKVersion, deviceInfo["libxsp version"].get<string>().c_str());
    // setIntegerParam(ADMaxSizeX, ....);
    // setIntegerParam(ADMaxSizeY, ....);
    setIntegerParam(ADMinX, 0);
    setIntegerParam(ADMinY, 0);

    // Initialize our modules
    int numModules = detectorInfo["modules"].size();
    setIntegerParam(ADXSPD_NumModules, numModules);
    for (int i = 0; i < numModules; i++) {
        // TODO: Create module objects
    }

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

static const iocshArg XSPDConfigArg0 = {"Port name", iocshArgString};
static const iocshArg XSPDConfigArg1 = {"IP", iocshArgString};
static const iocshArg XSPDConfigArg2 = {"Control Port", iocshArgInt};
static const iocshArg XSPDConfigArg3 = {"Device ID", iocshArgString};

/* Array of config args */
static const iocshArg* const XSPDConfigArgs[] = {&XSPDConfigArg0, &XSPDConfigArg1, &XSPDConfigArg2,
                                                 &XSPDConfigArg3};

/* what function to call at config */
static void configXSPDCallFunc(const iocshArgBuf* args) {
    ADXSPDConfig(args[0].sval, args[1].sval, args[2].ival, args[3].sval);
}

/* Function definition */
static const iocshFuncDef configXSPD = {"ADXSPDConfig", 4, XSPDConfigArgs};

/* IOC register function */
static void XSPDRegister(void) { iocshRegister(&configXSPD, configXSPDCallFunc); }

/* external function for IOC registration */
extern "C" {
epicsExportRegistrar(XSPDRegister);
}
