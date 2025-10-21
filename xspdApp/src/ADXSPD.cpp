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

using json = nlohmann::json;
using namespace std;

/**
 * @brief C wrapper function called by iocsh to create an instance of the ADXSPD driver
 *
 * @param portName The name of the asyn port for this driver
 * @param ipPort The IP address and port of the XSPD device (e.g. 192.168.1.100:8080)
 * @param deviceId The device ID of the XSPD device to connect to (if NULL, connects to first device
 * found)
 * @return int asynStatus code
 */
extern "C" int ADXSPDConfig(const char* portName, const char* ipPort, const char* deviceId) {
    new ADXSPD(portName, ipPort, deviceId);
    return asynSuccess;
}

/**
 * @brief C wrapper function called on IOC exit to delete the ADXSPD driver instance
 *
 * @param pPvt Pointer to the ADXSPD driver instance
 */
static void exitCallbackC(void* pPvt) {
    ADXSPD* pXSPD = (ADXSPD*) pPvt;
    delete (pXSPD);
}

/**
 * @brief Wrapper C function passed to epicsThreadCreate to create acquisition thread
 *
 * @param drvPvt Pointer to instance of ADXSPD driver object
 */
static void acquisitionThreadC(void* drvPvt) {
    ADXSPD* pPvt = (ADXSPD*) drvPvt;
    pPvt->acquisitionThread();
}

/**
 * @brief Wrapper C function passed to epicsThreadCreate to create acquisition thread
 *
 * @param drvPvt Pointer to instance of ADXSPD driver object
 */
static void monitorThreadC(void* drvPvt) {
    ADXSPD* pPvt = (ADXSPD*) drvPvt;
    pPvt->monitorThread();
}

/**
 * @brief Makes a GET request to the XSPD API and returns the parsed JSON response
 *
 * @param endpoint The API endpoint to query
 * @return json Parsed JSON response from the API
 */
json ADXSPD::xspdGet(string endpoint) {
    const char* functionName = "xspdGet";
    // Make a GET request to the XSPD API

    string requestUri = this->apiUri + "/devices/" + this->deviceId + "/variables?path=" + endpoint;
    DEBUG_ARGS("Sending GET request to %s", requestUri.c_str());

    cpr::Response response = cpr::Get(cpr::Url(requestUri));

    if (response.status_code != 200) {
        ERR_ARGS("Failed to get data from %s: %s", endpoint.c_str(),
                 response.error.message.c_str());
        return json();
    }

    try {
        json parsedResponse = json::parse(response.text, nullptr, true, false, true);
        if (parsedResponse.empty()) {
            ERR_ARGS("Empty JSON response from %s", endpoint.c_str());
        }
        DEBUG_ARGS("Recv response for endpoint %s: %s", endpoint.c_str(),
                   parsedResponse.dump(4).c_str());
        return parsedResponse;
    } catch (json::parse_error& e) {
        ERR_ARGS("Failed to parse JSON response from %s: %s", endpoint.c_str(), e.what());
        return json();
    }
}

/**
 * @brief Makes a PUT request to the XSPD API to set a variable value
 *
 * @param endpoint The API endpoint to set
 * @param value The value to set
 * @return asynStatus success if the request was successful, else failure
 */

template <typename T>
asynStatus ADXSPD::xspdSet(string endpoint, T value) {
    const char* functionName = "xspdSet";
    // Make a PUT request to the XSPD API
    string requestUri = this->apiUri + "/devices/" + this->deviceId +
                        "/variables?path=" + endpoint + "&value=" + to_string(value);
    DEBUG_ARGS("Sending PUT request to %s with value %s", requestUri.c_str(),
               to_string(value).c_str());

    cpr::Response response = cpr::Put(cpr::Url(requestUri));

    if (response.status_code != 200) {
        ERR_ARGS("Failed to set data for %s: %s", endpoint.c_str(), response.error.message.c_str());
        return asynError;
    }

    return asynSuccess;
}

asynStatus ADXSPD::xspdCommand(string command) {
    const char* functionName = "xspdCommand";

    json commands = xspdGet("commands");
    bool commandFound = false;
    for (auto& cmd : commands["data"]) {
        if (cmd["name"] == command) {
            DEBUG_ARGS("Found command %s in device commands", command.c_str());
            commandFound = true;
            break;
        }
    }
    if (!commandFound) {
        ERR_ARGS("Command %s not found in device commands", command.c_str());
        return asynError;
    }

    // Make a POST request to the XSPD API
    string requestUri = this->apiUri + "/devices/" + this->deviceId + "/command?path=" + command;
    DEBUG_ARGS("Sending POST request to %s", requestUri.c_str());

    cpr::Response response = cpr::Post(cpr::Url(requestUri));

    if (response.status_code != 200) {
        ERR_ARGS("Failed to send command %s: %s", command.c_str(), response.error.message.c_str());
        return asynError;
    }

    return asynSuccess;
}

// -----------------------------------------------------------------------
// ADXSPD Acquisition Functions
// -----------------------------------------------------------------------

/**
 * @brief Starts acquisition
 */
void ADXSPD::acquireStart() {
    const char* functionName = "acquireStart";
    setIntegerParam(ADAcquire, 1);
}

/**
 * @brief stops acquisition by aborting exposure and joinging acq thread
 */
void ADXSPD::acquireStop() {
    const char* functionName = "acquireStop";
    setIntegerParam(ADAcquire, 0);
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

    getIntegerParam(NDDataType, (int*) &dataType);
    getIntegerParam(ADImageMode, (int*) &acquisitionMode);

    size_t dims[2];
    getIntegerParam(ADSizeX, (int*) &dims[0]);
    getIntegerParam(ADSizeY, (int*) &dims[1]);

    int collectedImages = 0;

    while (this->alive) {
        int targetNumImages;
        getIntegerParam(ADNumImages, &targetNumImages);
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
        setIntegerParam(NDArraySize, (int) arrayInfo.totalBytes);
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
        if (acquisitionMode == ADImageSingle ||
            acquisitionMode == ADImageMultiple && collectedImages == targetNumImages) {
            acquireStop();
            collectedImages = 0;
        }
        // Release the array
        pArray->release();

        // refresh all PVs
        callParamCallbacks();
    }

    callParamCallbacks();
}

/**
 * Main monitoring function for ADXSPD
 */
void ADXSPD::monitorThread() {
    const char* functionName = "monitorThread";

    while (this->alive) {
        // Poll some status variables from the detector here
        epicsThreadSleep(5.0);
    }
}

//-------------------------------------------------------------------------
// ADDriver function overwrites
//-------------------------------------------------------------------------

/**
 * Override of ADDriver base function - callback whenever an int32 PV is written to
 *
 * @param pasynUser asyn client who requests a write
 * @param value int32 value to written to the parameter
 * @return asynStatus success if write was successful, else failure
 */
asynStatus ADXSPD::writeInt32(asynUser* pasynUser, epicsInt32 value) {
    int function = pasynUser->reason;
    int acquiring;
    int status = asynSuccess;
    static const char* functionName = "writeInt32";
    getIntegerParam(ADAcquire, &acquiring);

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

/**
 * @brief Override of ADDriver base function - reports driver status
 *
 * @param fp File pointer to write report to
 * @param details Level of detail for the report
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

/**
 * Constructor for the ADXSPD driver
 *
 * @param portName The name of the asyn port for this driver
 * @param ipPort The IP address and port of the XSPD device (e.g. 192.168.1.100:8080)
 * @param deviceId The device ID of the XSPD device to connect to (if NULL, connects to first device
 * found)
 */
ADXSPD::ADXSPD(const char* portName, const char* ipPort, const char* deviceId)
    : ADDriver(portName, 1, (int) NUM_ADXSPD_PARAMS, 0, 0, 0, 0, 0, 1, 0, 0) {
    static const char* functionName = "ADXSPD";
    cpr::Response response;

    // Create ADXSPD specific parameters
    createAllParams();

    // Sets driver version PV (version numbers defined in header file)
    char versionString[25];
    epicsSnprintf(versionString, sizeof(versionString), "%d.%d.%d", ADXSPD_VERSION, ADXSPD_REVISION,
                  ADXSPD_MODIFICATION);
    setStringParam(NDDriverVersion, versionString);

    // Initialize vendor SDK and connect to the device here
    string baseApiUri = string(ipPort) + "/api";
    INFO_ARGS("Connecting to XSPD api at %s", baseApiUri.c_str());

    response = cpr::Get(cpr::Url(baseApiUri));
    if (response.status_code != 200) {
        ERR_ARGS("Failed to connect to XSPD API at %s: %s", baseApiUri.c_str(),
                 response.error.message.c_str());
        return;
    }

    json xspdVersionInfo = json::parse(response.text, nullptr, true, false, true);
    if (xspdVersionInfo.empty()) {
        ERR("Failed to retrieve XSPD version information.");
        return;
    }
    string apiVersion = xspdVersionInfo["api version"].get<string>();
    string xspdVersion = xspdVersionInfo["xspd version"].get<string>();
    setStringParam(ADXSPD_ApiVersion, apiVersion.c_str());
    setStringParam(ADXSPD_Version, xspdVersion.c_str());

    INFO_ARGS("Connected to XSPD API, version %s", xspdVersion.c_str());
    this->apiUri = baseApiUri + "/v" + apiVersion;

    response = cpr::Get(cpr::Url(this->apiUri + "/devices"));
    if (response.status_code != 200) {
        ERR_ARGS("Failed to get device list from %s: %s", (this->apiUri + "/devices").c_str(),
                 response.error.message.c_str());
        return;
    }
    json deviceList = json::parse(response.text, nullptr, true, false, true);
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
    setStringParam(ADSerialNumber, this->deviceId.c_str());

    INFO_ARGS("Retrieving info for device with ID %s...", this->deviceId.c_str());

    response = cpr::Get(cpr::Url(this->apiUri + "/devices/" + this->deviceId));
    if (response.status_code != 200) {
        ERR_ARGS("Failed to get device info from %s: %s",
                 (this->apiUri + "/devices/" + this->deviceId).c_str(),
                 response.error.message.c_str());
        return;
    }
    json deviceInfo = json::parse(response.text, nullptr, true, false, true);
    if (deviceInfo.empty()) {
        ERR_ARGS("Failed to retrieve device info for device ID %s", this->deviceId.c_str());
        return;
    }

    // Assume we have a single data port (i.e. image stitching xspd-side)
    json dataPortInfo = deviceInfo["system"]["data-ports"][0];
    if (dataPortInfo.empty()) {
        ERR_ARGS("No data ports found for device ID %s", this->deviceId.c_str());
        return;
    }
    this->dataPortId = dataPortInfo["id"].get<string>();
    this->dataPortIp = dataPortInfo["ip"].get<string>();
    this->dataPortPort = dataPortInfo["port"].get<int>();
    INFO_ARGS("Found data port w/ id %s bound to %s:%d", this->dataPortId.c_str(),
              this->dataPortIp.c_str(), this->dataPortPort);

    json info = xspdGet("info");
    if (info.empty()) {
        ERR_ARGS("Failed to retrieve device info for device ID %s", this->deviceId.c_str());
        return;
    }

    // For now only support single-detector devices
    json detectorInfo = info["detectors"][0];
    this->detectorId = detectorInfo["id"].get<string>();

    for (auto& module : detectorInfo["modules"]) {
        INFO_ARGS("Found module %s, type %s", module["id"].get<string>().c_str(),
                  module["type"].get<string>().c_str());
        // TODO: Create module objects here
    }

    // Retrieve device information and populate all PVs.
    setStringParam(ADManufacturer, "X-Spectrum GmbH");
    setStringParam(ADModel, this->xspdGet("type")["value"].get<string>().c_str());
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

    epicsThreadOpts opts;
    opts.priority = epicsThreadPriorityHigh;
    opts.stackSize = epicsThreadGetStackSize(epicsThreadStackBig);
    opts.joinable = 1;
    this->acquisitionThreadId = epicsThreadCreateOpt(
        "acquisitionThread", (EPICSTHREADFUNC) acquisitionThreadC, this, &opts);

    epicsThreadOpts monitorOpts;
    monitorOpts.priority = epicsThreadPriorityMedium;
    monitorOpts.stackSize = epicsThreadGetStackSize(epicsThreadStackMedium);
    monitorOpts.joinable = 1;
    this->monitorThreadId =
        epicsThreadCreateOpt("monitorThread", (EPICSTHREADFUNC) monitorThreadC, this, &monitorOpts);

    // when epics is exited, delete the instance of this class
    epicsAtExit(exitCallbackC, this);
}

ADXSPD::~ADXSPD() {
    const char* functionName = "~ADXSPD";

    INFO("Shutting down ADXSPD driver...");

    this->alive = false;
    if (this->acquisitionThreadId != NULL) {
        INFO("Waiting for acquisition thread to join...");
        epicsThreadMustJoin(this->acquisitionThreadId);
    }
    if (this->monitorThreadId != NULL) {
        INFO("Waiting for monitoring thread to join...");
        epicsThreadMustJoin(this->monitorThreadId);
    }
    INFO("Done.");
}

//-------------------------------------------------------------
// ADXSPD ioc shell registration
//-------------------------------------------------------------

static const iocshArg XSPDConfigArg0 = {"Port name", iocshArgString};
static const iocshArg XSPDConfigArg1 = {"IP Port", iocshArgString};
static const iocshArg XSPDConfigArg2 = {"Device ID", iocshArgString};

/* Array of config args */
static const iocshArg* const XSPDConfigArgs[] = {&XSPDConfigArg0, &XSPDConfigArg1, &XSPDConfigArg2};

/* what function to call at config */
static void configXSPDCallFunc(const iocshArgBuf* args) {
    ADXSPDConfig(args[0].sval, args[1].sval, args[2].sval);
}

/* Function definition */
static const iocshFuncDef configXSPD = {"ADXSPDConfig", 3, XSPDConfigArgs};

/* IOC register function */
static void ADXSPDRegister(void) { iocshRegister(&configXSPD, configXSPDCallFunc); }

/* external function for IOC registration */
extern "C" {
epicsExportRegistrar(ADXSPDRegister);
}
