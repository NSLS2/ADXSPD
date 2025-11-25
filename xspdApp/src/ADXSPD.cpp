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

#include "ADXSPDModule.h"

using json = nlohmann::json;

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
 * @param uri The full URI to make the GET request to
 * @return json Parsed JSON response from the API
 */
json ADXSPD::xspdGet(string uri) {
    // Make a GET request to the XSPD API

    DEBUG_ARGS("Sending GET request to %s", uri.c_str());

    this->lock();
    cpr::Response response = cpr::Get(cpr::Url(uri));
    this->unlock();

    if (response.status_code != 200) {
        ERR_ARGS("Failed to get data from %s: %s", uri.c_str(), response.error.message.c_str());
        return json();
    }

    try {
        json parsedResponse = json::parse(response.text, nullptr, true, false, true);
        if (parsedResponse.empty()) {
            ERR_ARGS("Empty JSON response from %s", uri.c_str());
            return json();
        }
        DEBUG_ARGS("Recv response for endpoint %s: %s", uri.c_str(),
                   parsedResponse.dump(4).c_str());
        return parsedResponse;
    } catch (json::parse_error& e) {
        ERR_ARGS("Failed to parse JSON response from %s: %s", uri.c_str(), e.what());
        return json();
    }
}

template <typename T>
T ADXSPD::xspdGetVar(string endpoint, string key) {
    string fullVarEndpoint = this->deviceVarUri + endpoint;

    json response = xspdGet(fullVarEndpoint);

    if (response.is_null() || response.empty()) {
        ERR_ARGS("Failed to get variable %s", endpoint.c_str());
        return T();
    } else {
        if (response.contains(key)) {
            if constexpr (is_enum<T>::value) {
                string valAsStr = response[key].get<string>();
                auto enumValue = magic_enum::enum_cast<T>(valAsStr, magic_enum::case_insensitive);
                if (enumValue.has_value()) {
                    return enumValue.value();
                } else {
                    ERR_ARGS("Failed to cast value %s to enum for variable %s", valAsStr.c_str(), endpoint.c_str());
                    return T(0);
                }
            } else {
                return response[key].get<T>();
            }
        } else {
            ERR_ARGS("Key %s not found in response for variable %s", key.c_str(), endpoint.c_str());
            return T();
        }
    }
}

template <typename T>
T ADXSPD::xspdGetDetVar(string endpoint, string key) {
    string fullVarEndpoint = this->detectorId + "/" + endpoint;

    return xspdGetVar<T>(fullVarEndpoint, key);
}

template <typename T>
T ADXSPD::xspdGetDataPortVar(string endpoint, string key) {
    string fullVarEndpoint = this->dataPortId + "/" + endpoint;

    return xspdGetVar<T>(fullVarEndpoint, key);
}

/**
 * @brief Makes a PUT request to the XSPD API to set a variable value
 *
 * @param endpoint The API endpoint to set
 * @param value The value to set
 * @return asynStatus success if the request was successful, else failure
 */
template <typename T>
T ADXSPD::xspdSetVar(string endpoint, T value, string rbKey) {

    string valueAsStr;
    if constexpr (std::is_same_v<T, std::string>) {
        valueAsStr = value;
    } else if constexpr(std::is_enum<T>::value) {
        auto enumString = magic_enum::enum_name(value);
        if (enumString.empty()) {
            ERR_ARGS("Failed to convert enum value to string for variable %s", endpoint.c_str());
            return T(0);
        }
        valueAsStr = string(enumString);
    } else {
        valueAsStr = to_string(value);
    }

    string requestUri = this->apiUri + "/devices/" + this->deviceId +
                        "/variables?path=" + endpoint + "&value=" + valueAsStr;

    DEBUG_ARGS("Sending PUT request to %s with value %s", requestUri.c_str(),
           valueAsStr.c_str());

    this->lock();
    cpr::Response response = cpr::Put(cpr::Url(requestUri));
    this->unlock();

    if (response.status_code != 200) {
        ERR_ARGS("Failed to set variable %s", endpoint.c_str());
        return T();
    }
    cout << response.text << endl;

    json respJson = json::parse(response.text, nullptr, true, false, true);

    if (respJson.is_null() || respJson.empty()) {
        ERR_ARGS("Empty JSON response when setting variable %s", endpoint.c_str());
        return T();
    } else if (!rbKey.empty() && respJson.contains(rbKey)) {
        // Handle readback key if provided
        if constexpr (std::is_enum<T>::value) {
            auto enumValue = magic_enum::enum_cast<T>(respJson[rbKey].get<std::string>(),
                                                      magic_enum::case_insensitive);
            if (enumValue.has_value()) {
                return enumValue.value();
            } else {
                ERR_ARGS("Failed to cast readback value %s to enum for variable %s",
                         respJson[rbKey].get<std::string>().c_str(), endpoint.c_str());
                return T(0);
            }
        }
        else {
            return respJson[rbKey].get<T>();
        }
    }

    return T();
}

template <typename T>
T ADXSPD::xspdSetDetVar(string endpoint, T value, string rbKey) {
    string fullVarEndpoint = this->detectorId + "/" + endpoint;

    return xspdSetVar<T>(fullVarEndpoint, value, rbKey);
}


asynStatus ADXSPD::xspdCommand(string command) {
    json commands = xspdGet(this->deviceUri + "commands");
    bool commandFound = false;
    for (auto& cmd : commands) {
        if (cmd["path"] == command) {
            DEBUG_ARGS("Found command %s in device commands", command.c_str());
            commandFound = true;
            break;
        }
    }
    if (!commandFound) {
        ERR_ARGS("Command %s not found in device commands", command.c_str());
        return asynError;
    }

    // Make a PUT request to the XSPD API
    string requestUri = this->deviceUri + "command?path=" + command;
    DEBUG_ARGS("Sending PUT request to %s", requestUri.c_str());

    this->lock();
    cpr::Response response = cpr::Put(cpr::Url(requestUri));
    this->unlock();

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
    setIntegerParam(ADAcquire, 1);
    setIntegerParam(ADNumImagesCounter, 0);
    callParamCallbacks();
    xspdCommand(this->detectorId + "/start");

}

/**
 * @brief stops acquisition by aborting exposure and joinging acq thread
 */
void ADXSPD::acquireStop() {
    setIntegerParam(ADAcquire, 0);
    xspdCommand(this->detectorId + "/stop");
}

/**
 * Main acquisition function for ADXSPD
 */
void ADXSPD::acquisitionThread() {
    NDArray* pArray = nullptr;
    ADImageMode_t acquisitionMode;
    NDArrayInfo arrayInfo;
    // TODO: Support other data types
    NDDataType_t dataType;
    NDColorMode_t colorMode = NDColorModeMono;  // Only monochrome is supported.
    ADXSPDCounterMode counterMode;

    size_t dims[2];
    getIntegerParam(ADSizeX, (int*) &dims[0]);
    getIntegerParam(ADSizeY, (int*) &dims[1]);

    int collectedImages;

    void* zmqSubscriber = zmq_socket(this->zmqContext, ZMQ_SUB);
    int rc = zmq_connect(
        zmqSubscriber,
        (string("tcp://") + this->dataPortIp + ":" + to_string(this->dataPortPort)).c_str());
    if (rc != 0) {
        ERR_ARGS("Failed to connect to data port zmq socket at %s:%d", this->dataPortIp.c_str(),
                 this->dataPortPort);
        return;
    }
    // Subscribe to all messages
    rc = zmq_setsockopt(zmqSubscriber, ZMQ_SUBSCRIBE, "", 0);
    if (rc != 0) {
        ERR_ARGS("Failed to set zmq socket options for data port at %s:%d",
                 this->dataPortIp.c_str(), this->dataPortPort);
        return;
    }

    INFO_ARGS("Connected to data port zmq socket at %s:%d", this->dataPortIp.c_str(),
              this->dataPortPort);

    while (this->alive) {

        // Get a new frame using the vendor SDK here here
        vector<zmq_msg_t> frameMessages;
        int more;
        size_t moreSize = sizeof(more);
        do {
            zmq_msg_t messagePart;
            zmq_msg_init(&messagePart);

            int rc = zmq_msg_recv(&messagePart, zmqSubscriber, 0);
            if (rc == -1) {
                if (errno == ETERM) {
                    // Context terminated, exit thread
                    INFO("ZMQ context terminated, exiting acquisition thread...");
                    zmq_msg_close(&messagePart);
                    for(auto& msgPart : frameMessages) {
                        zmq_msg_close(&msgPart);
                    }
                    zmq_close(zmqSubscriber);
                    return;
                }
                ERR("Failed to receive ZMQ message");
                break;
            }

            frameMessages.push_back(messagePart);
            printf("Received message part of size %zu\n", zmq_msg_size(&messagePart));

            zmq_getsockopt(zmqSubscriber, ZMQ_RCVMORE, &more, &moreSize);
        } while (more);

        getIntegerParam(ADImageMode, (int*) &acquisitionMode);
        getIntegerParam(ADXSPD_CounterMode, (int*) &counterMode);

        int targetNumImages;
        getIntegerParam(ADNumImages, &targetNumImages);
        getIntegerParam(ADNumImagesCounter, &collectedImages);

        if (frameMessages.size() != 3) {
            ERR_ARGS("Expected 3 message parts for frame, got %zu", frameMessages.size());
            continue;
        } else {
            uint16_t frameNumber = *((uint16_t*) zmq_msg_data(&frameMessages[1]));
            uint16_t triggerNumber = *((uint16_t*) zmq_msg_data(&frameMessages[1]) + 1);
            uint16_t statusCode = *((uint16_t*) zmq_msg_data(&frameMessages[1]) + 2);
            uint16_t size = *((uint16_t*) zmq_msg_data(&frameMessages[1]) + 3);

            // The third message part contains the frame data
            // TODO: This assumes no compression; handle compressed data later
            size_t frameSizeBytes = zmq_msg_size(&frameMessages[2]);

            DEBUG_ARGS(
                "Received frame number %d, trigger number %d, status code %d, size %d, %ld bytes",
                frameNumber, triggerNumber, statusCode, size, frameSizeBytes);

            getIntegerParam(NDDataType, (int*) &dataType);

            // Allocate the NDArray of the correct size
            this->pArrays[0] =
                pNDArrayPool->alloc(2, (size_t*) dims, dataType, frameSizeBytes, NULL);

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
            memcpy(pArray->pData, zmq_msg_data(&frameMessages[2]), arrayInfo.totalBytes);

            // increment the array counter
            int arrayCounter;
            getIntegerParam(NDArrayCounter, &arrayCounter);
            arrayCounter++;
            setIntegerParam(NDArrayCounter, arrayCounter);

            // set the image unique ID to the number in the sequence
            pArray->uniqueId = static_cast<int>(frameNumber);
            pArray->pAttributeList->add("ColorMode", "Color Mode", NDAttrInt32, &colorMode);

            getAttributes(pArray->pAttributeList);
            doCallbacksGenericPointer(pArray, NDArrayData, 0);

            // If in single mode, finish acq, if in multiple mode and reached target number
            // complete acquisition.
            if (acquisitionMode == ADImageSingle ||
                (acquisitionMode == ADImageMultiple && collectedImages == (targetNumImages * pow(2, (int) counterMode)))) {
                acquireStop();
            }
            // Release the array
            pArray->release();
        }

        // Release ZMQ messages
        for (auto& msgPart : frameMessages) {
            zmq_msg_close(&msgPart);
        }

        // refresh all PVs
        callParamCallbacks();
    }

    if(zmqSubscriber != nullptr) {
        INFO("Closing zmq subscriber socket...");
        zmq_close(zmqSubscriber);
    }
}

/**
 * @brief Main monitoring loop for ADXSPD
 */
void ADXSPD::monitorThread() {
    while (this->alive) {
        ADXSPDStatus status =
            xspdGetDetVar<ADXSPDStatus>("status");
        setIntegerParam(ADStatus, static_cast<int>(status));

        if (status != ADXSPDStatus::BUSY) {
            // Lock the driver here, so we can't start
            // an acquisition while reading module statuses
            this->lock();
            for (auto& module : this->modules) {
                module->checkStatus();
            }
            this->unlock();
        }

        // TODO: Allow for setting the polling interval via a PV
        epicsThreadSleep(10.0);
        callParamCallbacks();
    }
}

NDDataType_t ADXSPD::getDataTypeForBitDepth(int bitDepth) {
    switch (bitDepth) {
        case 1:
        case 6:
            return NDUInt8;
        case 12:
            return NDUInt16;
        case 24:
            return NDUInt32;
        default:
            throw std::invalid_argument("Unsupported bit depth");
    }
}

void ADXSPD::getInitialDetState() {
    setDoubleParam(ADAcquireTime, xspdGetDetVar<double>("shutter_time") / 1000.0);  // XSPD API uses milliseconds
    setIntegerParam(ADXSPD_SummedFrames, xspdGetDetVar<int>("summed_frames"));
    setIntegerParam(ADXSPD_RoiRows, xspdGetDetVar<int>("roi_rows"));

    // Set ADNumImages and ADImageMode based on n_frames
    int numImages = xspdGetDetVar<int>("n_frames");
    setIntegerParam(ADNumImages, numImages);
    if (numImages == 1)
        setIntegerParam(ADImageMode, ADImageSingle);
    else
        setIntegerParam(ADImageMode, ADImageMultiple);

    setIntegerParam(ADXSPD_CompressLevel, xspdGetDetVar<int>("compression_level"));
    setIntegerParam(ADXSPD_Compressor,
                    static_cast<int>(xspdGetDetVar<ADXSPDCompressor>("compressor")));
    setDoubleParam(ADXSPD_BeamEnergy, xspdGetDetVar<double>("beam_energy"));
    setIntegerParam(ADXSPD_GatingMode,
                    static_cast<int>(xspdGetDetVar<ADXSPDOnOff>("gating_mode")));
    setIntegerParam(ADXSPD_FFCorrection,
                    static_cast<int>(xspdGetDetVar<ADXSPDOnOff>("flatfield_correction")));
    setIntegerParam(ADXSPD_ChargeSumming,
                    static_cast<int>(xspdGetDetVar<ADXSPDOnOff>("charge_summing")));
    setIntegerParam(ADTriggerMode,
                    static_cast<int>(xspdGetDetVar<ADXSPDTrigMode>("trigger_mode")));

    int bitDepth = xspdGetDetVar<int>("bit_depth");
    setIntegerParam(ADXSPD_BitDepth, bitDepth);
    setIntegerParam(NDDataType, static_cast<int>(getDataTypeForBitDepth(bitDepth)));

    setIntegerParam(ADXSPD_CrCorr,
                    static_cast<int>(xspdGetDetVar<ADXSPDOnOff>("countrate_correction")));
    setIntegerParam(ADXSPD_CounterMode,
                    static_cast<int>(xspdGetDetVar<ADXSPDCounterMode>("counter_mode")));
    setIntegerParam(ADXSPD_SaturationFlag,
                    static_cast<int>(xspdGetDetVar<ADXSPDOnOff>("saturation_flag")));
    setIntegerParam(ADXSPD_ShuffleMode,
                    static_cast<int>(xspdGetDetVar<ADXSPDShuffleMode>("shuffle_mode")));

    setStringParam(ADModel, xspdGetDetVar<string>("type").c_str());

    vector<double> thresholds = xspdGetDetVar<vector<double>>("thresholds");
    if (thresholds.size() > 0) setDoubleParam(ADXSPD_LowThreshold, thresholds[0]);
    if (thresholds.size() > 1) setDoubleParam(ADXSPD_HighThreshold, thresholds[1]);

    int maxSizeX = xspdGetDataPortVar<int>("frame_width");
    int maxSizeY = xspdGetDataPortVar<int>("frame_height");
    setIntegerParam(ADMaxSizeX, maxSizeX);
    setIntegerParam(ADMaxSizeY, maxSizeY);
    setIntegerParam(ADSizeX, maxSizeX);
    setIntegerParam(ADSizeY, maxSizeY);
    setIntegerParam(ADBinX, 1);
    setIntegerParam(ADBinY, 1);
    setIntegerParam(ADMinX, 0);
    setIntegerParam(ADMinY, 0);
    setIntegerParam(NDColorMode, NDColorModeMono);

    callParamCallbacks();
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
    getIntegerParam(ADAcquire, &acquiring);

    const char* paramName;
    getParamName(function, &paramName);

    if (acquiring && find(this->onlyIdleParams.begin(), this->onlyIdleParams.end(), function) !=
                         this->onlyIdleParams.end()) {
        ERR_ARGS("Cannot set parameter %s while acquiring", paramName);
        return asynError;
    }

    // start/stop acquisition
    if (function == ADAcquire) {
        if (value && !acquiring) {
            acquireStart();
        } else if (!value && acquiring) {
            acquireStop();
        }
    } else if (function == ADImageMode) {
        switch (value) {
            case ADImageSingle:
                setIntegerParam(ADNumImages, xspdSetDetVar<int>("n_frames", 1));
            case ADImageMultiple:
                // Leave ADNumImages unchanged
                setIntegerParam(ADImageMode, value);
                break;
            case ADImageContinuous:
                ERR("Continuous acquisition is not supported!");
                break;
        }
    } else if (function == ADNumImages) {
        int maxNumImages = INT_MAX;
        for (auto& module : this->modules) {
            int moduleMax = module->getMaxNumImages();
            if (moduleMax < maxNumImages) {
                maxNumImages = moduleMax;
            }
        }
        if (value < 1 || value > maxNumImages) {
            ERR_ARGS("Invalid number of images: %d (valid range: 1-%d)", value, maxNumImages);
            return asynError;
        }
        setIntegerParam(ADNumImages, xspdSetDetVar<int>("n_frames", value));
        if (value == 1)
            setIntegerParam(ADImageMode, ADImageSingle);
        else
            setIntegerParam(ADImageMode, ADImageMultiple);
    } else if (function < ADXSPD_FIRST_PARAM) {
        status = ADDriver::writeInt32(pasynUser, value);
    } else {
        int actualValue;
        string endpoint;
        if(function == ADXSPD_BitDepth) {
            actualValue = xspdSetDetVar<int>("bit_depth", value);
        } else if(function == ADXSPD_SummedFrames) {
            actualValue = xspdSetDetVar<int>("summed_frames", value);
        } else if(function == ADXSPD_RoiRows) {
            actualValue = xspdSetDetVar<int>("roi_rows", value);
        } else if(function == ADXSPD_CompressLevel) {
            actualValue = xspdSetDetVar<int>("compression_level", value);
        } else if(function == ADXSPD_GatingMode) {
            actualValue = xspdSetDetVar<int>("gating_mode", value);
            actualValue = static_cast<int>(xspdSetDetVar<ADXSPDOnOff>("gating_mode", static_cast<ADXSPDOnOff>(value)));
        } else if(function == ADXSPD_FFCorrection) {
            actualValue = static_cast<int>(xspdSetDetVar<ADXSPDOnOff>("flatfield_correction", static_cast<ADXSPDOnOff>(value)));
        } else if(function == ADXSPD_ChargeSumming) {
            actualValue = static_cast<int>(xspdSetDetVar<ADXSPDOnOff>("charge_summing", static_cast<ADXSPDOnOff>(value)));
        } else if(function == ADTriggerMode) {
            actualValue = static_cast<int>(xspdSetDetVar<ADXSPDTrigMode>("trigger_mode", static_cast<ADXSPDTrigMode>(value)));
        } else if(function == ADXSPD_CrCorr) {
            actualValue = static_cast<int>(xspdSetDetVar<ADXSPDOnOff>("countrate_correction", static_cast<ADXSPDOnOff>(value)));
        } else if(function == ADXSPD_CounterMode) {
            actualValue = static_cast<int>(xspdSetDetVar<ADXSPDCounterMode>("counter_mode", static_cast<ADXSPDCounterMode>(value)));
        } else if(function == ADXSPD_SaturationFlag) {
            actualValue = static_cast<int>(xspdSetDetVar<ADXSPDOnOff>("saturation_flag", static_cast<ADXSPDOnOff>(value)));
        } else if(function == ADXSPD_ShuffleMode) {
            actualValue = static_cast<int>(xspdSetDetVar<ADXSPDShuffleMode>("shuffle_mode", static_cast<ADXSPDShuffleMode>(value)));
        } else {
            WARN_ARGS("Unhandled parameter write event for param %s", paramName);
            return asynError;
        }

        setIntegerParam(function, actualValue);
        if(actualValue != value) {
            WARN_ARGS("Requested value %d for parameter %s, but set value is %d", value, paramName, actualValue);
            status = asynError;
        }
    }
    callParamCallbacks();

    if (status) {
        ERR_ARGS("status=%d, parameter=%s, value=%d", status, paramName, value);
        return asynError;
    } else {
        DEBUG_ARGS("parameter=%s value=%d", paramName, value);
        return asynSuccess;
    }
}


double ADXSPD::setThreshold(ADXSPDThreshold threshold, double value) {

    string thresholdName = (threshold == ADXSPDThreshold::LOW) ? "Low" : "High";

    vector<double> thresholds = xspdGetDetVar<vector<double>>("thresholds");
    if (thresholds.size() == 0 && threshold != ADXSPDThreshold::LOW) {
        ERR("Must set low threshold before setting high threshold");
        return 0.0;
    }

    switch(thresholds.size()) {
        case 2:
            thresholds[static_cast<int>(threshold)] = value;
            break;
        case 1:
            if (threshold == ADXSPDThreshold::LOW) {
                thresholds[0] = value;
            } else {
                thresholds.push_back(value);
            }
            break;
        case 0:
            thresholds.push_back(value);
            break;
        default:
            break;
    }

    string thresholdsStr = "";
    for (size_t i = 0; i < thresholds.size(); i++) {
        thresholdsStr += to_string(thresholds[i]);
        if (i < thresholds.size() - 1) {
            thresholdsStr += ",";
        }
    }

    // Thresholds set as comma-separated string, read as vector<double>
    string rbThresholdsStr = xspdSetDetVar<string>("thresholds", thresholdsStr, "thresholds");
    vector<double> rbThresholds = json::parse(rbThresholdsStr.c_str()).get<vector<double>>();

    if(rbThresholds.size() <= static_cast<size_t>(threshold)) {
        ERR_ARGS("Failed to set %s threshold, readback size is less than expected", thresholdName.c_str());
        return 0.0;
    }

    return rbThresholds[static_cast<size_t>(threshold)];
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
    getIntegerParam(ADAcquire, &acquiring);
    double actualValue;

    const char* paramName;
    getParamName(function, &paramName);

    if (acquiring && find(this->onlyIdleParams.begin(), this->onlyIdleParams.end(), function) !=
                         this->onlyIdleParams.end()) {
        ERR_ARGS("Cannot set parameter %s while acquiring", paramName);
        return asynError;
    }

    if (function == ADAcquireTime) {
        actualValue = xspdSetDetVar<double>("shutter_time", value * 1000.0) /
                      1000.0;  // XSPD API uses milliseconds
        setDoubleParam(ADAcquireTime, actualValue);
    } else if (function < ADXSPD_FIRST_PARAM) {
        status = ADDriver::writeFloat64(pasynUser, value);
    } else {
        double actualValue;
        string endpoint;
        if(function == ADXSPD_BeamEnergy) {
            endpoint = "beam_energy";
            actualValue = xspdSetDetVar<double>(endpoint, value);
        } else if(function == ADXSPD_LowThreshold) {
            double actualValue = setThreshold(ADXSPDThreshold::LOW, value);
        } else if(function == ADXSPD_HighThreshold) {
            double actualValue = setThreshold(ADXSPDThreshold::HIGH, value);
        } else {
            WARN_ARGS("Unhandled parameter write event for param %s", paramName);
            return asynError;
        }
        setDoubleParam(function, actualValue);
        if(actualValue != value) {
            WARN_ARGS("Requested value %f for parameter %s, but set value is %f", value, paramName, actualValue);
            status = asynError;
        }
    }

    callParamCallbacks();

    if (status) {
        ERR_ARGS("status=%d, parameter=%s, value=%f", status, paramName, value);
        return asynError;
    } else {
        DEBUG_ARGS("parameter=%s value=%f", paramName, value);
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
    cpr::Response response;

    // Create ADXSPD specific parameters
    createAllParams();

    // Sets driver version PV (version numbers defined in header file)
    char versionString[25];
    snprintf(versionString, sizeof(versionString), "%d.%d.%d", ADXSPD_VERSION, ADXSPD_REVISION,
             ADXSPD_MODIFICATION);
    setStringParam(NDDriverVersion, versionString);

    // Initialize vendor SDK and connect to the device here
    string baseApiUri = string(ipPort) + "/api";
    INFO_ARGS("Connecting to XSPD api at %s", baseApiUri.c_str());

    json xspdVersionInfo = xspdGet(baseApiUri);
    if (xspdVersionInfo.empty())
        throw std::runtime_error("Failed to connect to XSPD API.");

    string apiVersion = xspdVersionInfo["api version"].get<string>();
    string xspdVersion = xspdVersionInfo["xspd version"].get<string>();
    setStringParam(ADXSPD_ApiVersion, apiVersion.c_str());
    setStringParam(ADXSPD_Version, xspdVersion.c_str());

    INFO_ARGS("Connected to XSPD API, version %s", xspdVersion.c_str());
    this->apiUri = baseApiUri + "/v" + apiVersion;

    json devices = xspdGet(this->apiUri + "/devices");
    if (devices.is_null() || devices.empty() || !devices.contains("devices"))
        throw std::runtime_error("Failed to retrieve device list from XSPD API.");

    json deviceList = devices["devices"];
    if (deviceList.empty())
        throw std::runtime_error("No devices found in XSPD API device list.");

    json requestedDevice = json();
    INFO_ARGS("Found %ld device(s)", deviceList.size());

    if (deviceId == nullptr) {
        INFO("Device ID not specified, using first device in list.");
        requestedDevice = deviceList[0];
    } else {
        for (const auto& device : deviceList) {
            if (device.contains("id") && device["id"].get<string>().compare(deviceId) == 0) {
                requestedDevice = device;
                break;
            }
        }
    }

    if (requestedDevice.empty())
        throw std::runtime_error("Requested device ID not found in XSPD API device list.");

    this->deviceId = requestedDevice["id"].get<string>();
    this->deviceUri = this->apiUri + "/devices/" + this->deviceId;
    this->deviceVarUri = this->deviceUri + "/variables?path=";
    setStringParam(ADSerialNumber, this->deviceId.c_str());

    INFO_ARGS("Retrieving info for device with ID %s...", this->deviceId.c_str());

    json deviceInfo = xspdGet(this->deviceUri);
    if (deviceInfo.is_null() || deviceInfo.empty()) 
        throw std::runtime_error("Failed to retrieve device info from XSPD API.");

    if (!deviceInfo.contains("system") || !deviceInfo["system"].contains("data-ports") ||
        deviceInfo["system"]["data-ports"].empty())
        throw std::runtime_error("No data port information found for device in XSPD API.");

    // Always use the first data port (i.e. image stitching xspd-side)
    json dataPortInfo = deviceInfo["system"]["data-ports"][0];

    if (!dataPortInfo.contains("id") || !dataPortInfo.contains("ip") ||
        !dataPortInfo.contains("port"))
        throw std::runtime_error("Incomplete data port info for device in XSPD API.");

    this->dataPortId = dataPortInfo["id"].get<string>();
    this->dataPortIp = dataPortInfo["ip"].get<string>();
    this->dataPortPort = dataPortInfo["port"].get<int>();

    this->zmqContext = zmq_ctx_new();

    json info = xspdGetVar<json>("info");
    if (info.empty() || !info.contains("detectors") || info["detectors"].empty())
        throw std::runtime_error("No detector information found for device in XSPD API.");

    // For now only support single-detector devices
    json detectorInfo = info["detectors"][0];
    if (!detectorInfo.contains("detector-id"))
        throw std::runtime_error("Failed to find detector ID in XSPD API.");

    this->detectorId = detectorInfo["detector-id"].get<string>();

    // Retrieve device information and populate all PVs.
    setStringParam(ADManufacturer, "X-Spectrum GmbH");
    setStringParam(ADModel, this->detectorId.c_str());
    if (info.contains("libxsp version"))
        setStringParam(ADSDKVersion, info["libxsp version"].get<string>().c_str());

    // // Initialize our modules
    // int numModules = detectorInfo["modules"].size();
    // setIntegerParam(ADXSPD_NumModules, numModules);
    // for (int i = 0; i < numModules; i++) {
    //     json moduleInfo = detectorInfo["modules"][i];

    //     // Set firmware version from first module
    //     if (i == 0 && moduleInfo.contains("firmware"))
    //         setStringParam(ADFirmwareVersion, moduleInfo["firmware"].get<string>().c_str());

    //     string moduleId = moduleInfo["module"].get<string>();
    //     string modulePortName = string(this->portName) + "_MOD" + to_string(i + 1);
    //     INFO_ARGS("Initializing module %s on port %s", moduleId.c_str(), modulePortName.c_str());
    //     this->modules.push_back(new ADXSPDModule(modulePortName.c_str(), moduleId, this));
    // }

    // this->getInitialDetState();
    // callParamCallbacks();

    // epicsThreadOpts opts;
    // opts.priority = epicsThreadPriorityHigh;
    // opts.stackSize = epicsThreadGetStackSize(epicsThreadStackBig);
    // opts.joinable = 1;
    // this->acquisitionThreadId = epicsThreadCreateOpt(
    //     "acquisitionThread", (EPICSTHREADFUNC) acquisitionThreadC, this, &opts);

    // epicsThreadOpts monitorOpts;
    // monitorOpts.priority = epicsThreadPriorityMedium;
    // monitorOpts.stackSize = epicsThreadGetStackSize(epicsThreadStackMedium);
    // monitorOpts.joinable = 1;
    // this->monitorThreadId =
    //     epicsThreadCreateOpt("monitorThread", (EPICSTHREADFUNC) monitorThreadC, this, &monitorOpts);

    // when epics is exited, delete the instance of this class
    epicsAtExit(exitCallbackC, this);
}

ADXSPD::~ADXSPD() {
    INFO("Shutting down ADXSPD driver...");

    int acquiring;
    getIntegerParam(ADAcquire, &acquiring);
    if (acquiring) {
        INFO("Stopping acquisition...");
        acquireStop();
    }

    this->alive = false;

    if (this->monitorThreadId != NULL) {
        INFO("Waiting for monitoring thread to join...");
        epicsThreadMustJoin(this->monitorThreadId);
    }

    if (this->zmqContext != NULL) {
        INFO("Destroying zmq context...");
        zmq_ctx_destroy(this->zmqContext);
    }

    if (this->acquisitionThreadId != NULL) {
        INFO("Waiting for acquisition thread to join...");
        epicsThreadMustJoin(this->acquisitionThreadId);
    }

    for (auto& module : this->modules) {
        delete module;
    }

    this->shutdownPortDriver();

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
