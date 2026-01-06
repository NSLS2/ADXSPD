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
extern "C" int ADXSPDConfig(const char* portName, const char* ip, int portNum,
                            const char* deviceId) {
    new ADXSPD(portName, ip, portNum, deviceId);
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

// -----------------------------------------------------------------------
// ADXSPD Acquisition Functions
// -----------------------------------------------------------------------

/**
 * @brief Starts acquisition
 * 
 * @return asynStatus asynSuccess on success, asynError on failure
 */
asynStatus ADXSPD::acquireStart() {
    setIntegerParam(ADAcquire, 1);
    setIntegerParam(ADNumImagesCounter, 0);

    // Update frame size parameters from detector
    int sizeX = this->pDetector->GetActiveDataPort()->GetVar<int>("frame_width");
    int sizeY = this->pDetector->GetActiveDataPort()->GetVar<int>("frame_height");
    setIntegerParam(ADSizeX, sizeX);
    setIntegerParam(ADSizeY, sizeY);

    callParamCallbacks();

    // Acquire a lock to prevent other API calls during acquisition start
    this->lock();
    try {
        this->pDetector->ExecCommand("start");
    } catch (std::exception& e) {
        ERR_ARGS("Failed to start acquisition: %s", e.what());
        return asynError;
    }
    this->unlock();
    return asynSuccess;
}

/**
 * @brief stops acquisition by aborting exposure and joinging acq thread
 * 
 * @return asynStatus asynSuccess on success, asynError on failure
 */
asynStatus ADXSPD::acquireStop() {
    setIntegerParam(ADAcquire, 0);
    try {
        this->pDetector->ExecCommand("stop");
    } catch (std::exception& e) {
        ERR_ARGS("Failed to stop acquisition: %s", e.what());
        return asynError;
    }
    return asynSuccess;
}

/**
 * @brief Subtracts two frames element-wise with floor at 0
 *
 * @tparam T The data type of the frame pixels
 * @param currentFrame Pointer to the current frame data
 * @param previousFrame Pointer to the previous frame data
 * @param outputFrame Pointer to the output frame data
 * @param numBytes Number of bytes in each frame
 */
template <typename T>
void ADXSPD::subtractFrames(void* currentFrame, void* previousFrame, void* outputFrame, size_t numBytes) {

    // Cast the void pointers to the appropriate type
    T* currFrameT = static_cast<T*>(currentFrame);
    T* prevFrameT = static_cast<T*>(previousFrame);
    T* outFrameT = static_cast<T*>(outputFrame);

    // Calculate our number of elements, and then perform subtraction with floor at 0
    size_t numElements = numBytes / sizeof(T);
    for (size_t i = 0; i < numElements; i++) {
        T subtractedPixel = currFrameT[i] - prevFrameT[i];
        if (subtractedPixel < 0) {
            outFrameT[i] = T(0);
        } else {
            outFrameT[i] = subtractedPixel;
        }
    }
}

/**
 * @brief Main acquisition loop thread
 */
void ADXSPD::acquisitionThread() {
    NDArray* pArray = nullptr;
    ADImageMode_t acquisitionMode;
    NDArrayInfo arrayInfo;
    // TODO: Support other data types
    NDDataType_t dataType;
    NDColorMode_t colorMode = NDColorModeMono;  // Only monochrome is supported.
    XSPD::CounterMode counterMode;

    void* frameBuffer = nullptr;
    void* prevFrameBuffer = nullptr;

    int arrayCallbacks;

    int collectedImages;

    void* zmqSubscriber = zmq_socket(this->zmqContext, ZMQ_SUB);
    int rc = zmq_connect(zmqSubscriber, this->pDetector->GetActiveDataPort()->GetURI().c_str());
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

    // Run acquisition loop forever, until zmq context is terminated in the destructor
    while (true) {
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
                    for (auto& msgPart : frameMessages) {
                        zmq_msg_close(&msgPart);
                    }
                    frameMessages.clear();
                    zmq_close(zmqSubscriber);
                    return;
                }
                ERR("Failed to receive ZMQ message");
                break;
            }

            frameMessages.push_back(messagePart);
            DEBUG_ARGS("Received message part of size %zu\n", zmq_msg_size(&messagePart));

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

            // For reason's I don't fully understand, casting the dims address pointers to int*
            // causes them to be populated with really odd values...
            size_t dims[2];
            int sizeX, sizeY;
            getIntegerParam(ADSizeX, &sizeX);
            getIntegerParam(ADSizeY, &sizeY);
            dims[0] = (size_t) sizeX;
            dims[1] = (size_t) sizeY;

            // Allocate the NDArray of the correct size
            pArray = pNDArrayPool->alloc(2, dims, dataType, frameSizeBytes, NULL);

            if (!pArray) {
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

            getIntegerParam(NDArrayCallbacks, &arrayCallbacks);

            // Decompress data if compressed
            XSPD::Compressor compressor;
            getIntegerParam(ADXSPD_Compressor, (int*) &compressor);

            if (counterMode == XSPD::CounterMode::DUAL)
                frameBuffer = calloc(1, arrayInfo.totalBytes);
            else
                frameBuffer = pArray->pData;

            bool decompressOK = true;
            if (compressor == XSPD::Compressor::ZLIB) {
                // Decompress using zlib
                size_t decompressedSize;
                int zlibStatus =
                    uncompress((Bytef*) frameBuffer, &decompressedSize,
                               (Bytef*) zmq_msg_data(&frameMessages[2]), frameSizeBytes);
                if (zlibStatus != Z_OK) {
                    ERR_ARGS("Failed to decompress frame data with zlib, status code %d",
                             zlibStatus);
                }
                if (decompressedSize != arrayInfo.totalBytes) {
                    ERR_ARGS("Decompressed size %lu does not match expected size %lu",
                             decompressedSize, arrayInfo.totalBytes);
                }
            } else if (compressor == XSPD::Compressor::BLOSC) {
                size_t decompressSize;
                decompressSize = blosc_decompress(zmq_msg_data(&frameMessages[2]), frameBuffer,
                                                 arrayInfo.totalBytes);
                if (decompressSize != arrayInfo.totalBytes) {
                    ERR_ARGS("Failed to decompress frame data with Blosc, decompressed size %zu does not match expected size %zu",
                             decompressSize, arrayInfo.totalBytes);
                    decompressOK = false;
                }
            } else {
                // Copy data from new frame to pArray
                memcpy(frameBuffer, zmq_msg_data(&frameMessages[2]), arrayInfo.totalBytes);
            }

            bool subtractOk = true;
            if (counterMode == XSPD::CounterMode::DUAL && prevFrameBuffer != nullptr) {
                switch (dataType) {
                    case NDUInt8:
                        this->subtractFrames<uint8_t>(frameBuffer, prevFrameBuffer, pArray->pData,
                                                      arrayInfo.totalBytes);
                        break;
                    case NDUInt16:
                        this->subtractFrames<uint16_t>(frameBuffer, prevFrameBuffer, pArray->pData,
                                                       arrayInfo.totalBytes);
                        break;
                    case NDUInt32:
                        this->subtractFrames<uint32_t>(frameBuffer, prevFrameBuffer, pArray->pData,
                                                       arrayInfo.totalBytes);
                        break;
                    default:
                        ERR("Unsupported data type for frame subtraction");
                        subtractOk = false;
                        break;
                }
            } else if (counterMode == XSPD::CounterMode::DUAL && prevFrameBuffer == nullptr) {
                // Store the current frame as the previous frame for the next iteration
                prevFrameBuffer = frameBuffer;
                frameBuffer = nullptr;
                subtractOk = false;
            }

            if (decompressOK && subtractOk) {
                DEBUG("Copied frame to framebuffer.");

                // increment the array counter
                int arrayCounter;
                getIntegerParam(NDArrayCounter, &arrayCounter);
                arrayCounter++;
                setIntegerParam(NDArrayCounter, arrayCounter);

                // set the image unique ID to the number in the sequence
                pArray->uniqueId = arrayCounter;
                pArray->pAttributeList->add("ColorMode", "Color Mode", NDAttrInt32, &colorMode);

                getAttributes(pArray->pAttributeList);

                if (arrayCallbacks) doCallbacksGenericPointer(pArray, NDArrayData, 0);
            }

            // If in single mode, finish acq, if in multiple mode and reached target number
            // complete acquisition.
            if (acquisitionMode == ADImageSingle ||
                (acquisitionMode == ADImageMultiple &&
                 collectedImages == (targetNumImages * pow(2, (int) counterMode)))) {
                acquireStop();
            }
            pArray->release();
        }

        // Release ZMQ messages
        for (auto& msgPart : frameMessages) {
            zmq_msg_close(&msgPart);
        }

        // refresh all PVs
        callParamCallbacks();
    }

    if (zmqSubscriber != nullptr) {
        INFO("Closing zmq subscriber socket...");
        zmq_close(zmqSubscriber);
    }
}

/**
 * @brief Main monitoring loop for ADXSPD
 */
void ADXSPD::monitorThread() {
    double pollInterval;
    while (true) {
        getDoubleParam(ADXSPD_StatusInterval, &pollInterval);


        // Don't allow polling faster than minimum interval
        if (pollInterval <= ADXSPD_MIN_STATUS_POLL_INTERVAL)
            pollInterval = ADXSPD_MIN_STATUS_POLL_INTERVAL;

        XSPD::Status status = this->pDetector->GetVar<XSPD::Status>("status");
        int adStatus = ADStatusIdle;
        switch (status) {
            case XSPD::Status::READY:
                break;
            case XSPD::Status::BUSY:
                adStatus = ADStatusAcquire;
                break;
            case XSPD::Status::CONNECTED:
                adStatus = ADStatusInitializing;
                break;
            default:
                WARN("Detector status: UNKNOWN");
                break;
        }
        setIntegerParam(ADStatus, adStatus);

        if (status != XSPD::Status::BUSY) {
            // Lock the driver here, so we can't start
            // an acquisition while reading module statuses
            this->lock();
            for (auto& module : this->modules) {
                module->checkStatus();
            }
            this->unlock();
        }

        // TODO: Allow for setting the polling interval via a PV
        if(epicsEventWaitWithTimeout(this->shutdownEventId, pollInterval) == epicsEventWaitOK) {
            INFO("Shutdown event received, exiting monitor thread...");
            break;
        }

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
    try {
        setDoubleParam(ADAcquireTime, this->pDetector->GetVar<double>("shutter_time") /
                                          1000.0);  // XSPD API uses milliseconds
        setIntegerParam(ADXSPD_SummedFrames, this->pDetector->GetVar<int>("summed_frames"));

        // Set ADNumImages and ADImageMode based on n_frames
        int numImages = this->pDetector->GetVar<int>("n_frames");
        setIntegerParam(ADNumImages, numImages);
        if (numImages == 1)
            setIntegerParam(ADImageMode, ADImageSingle);
        else
            setIntegerParam(ADImageMode, ADImageMultiple);

        setIntegerParam(ADXSPD_CompressLevel, this->pDetector->GetVar<int>("compression_level"));
        setIntegerParam(ADXSPD_Compressor,
                        static_cast<int>(this->pDetector->GetVar<XSPD::Compressor>("compressor")));
        setDoubleParam(ADXSPD_BeamEnergy, this->pDetector->GetVar<double>("beam_energy"));
        setIntegerParam(ADXSPD_GatingMode,
                        static_cast<int>(this->pDetector->GetVar<XSPD::OnOff>("gating_mode")));
        setIntegerParam(
            ADXSPD_FFCorrection,
            static_cast<int>(this->pDetector->GetVar<XSPD::OnOff>("flatfield_correction")));
        setIntegerParam(ADXSPD_ChargeSumming,
                        static_cast<int>(this->pDetector->GetVar<XSPD::OnOff>("charge_summing")));
        setIntegerParam(
            ADTriggerMode,
            static_cast<int>(this->pDetector->GetVar<XSPD::TriggerMode>("trigger_mode")));

        int bitDepth = this->pDetector->GetVar<int>("bit_depth");
        setIntegerParam(ADXSPD_BitDepth, bitDepth);
        setIntegerParam(NDDataType, static_cast<int>(getDataTypeForBitDepth(bitDepth)));

        setIntegerParam(
            ADXSPD_CrCorr,
            static_cast<int>(this->pDetector->GetVar<XSPD::OnOff>("countrate_correction")));
        setIntegerParam(
            ADXSPD_CounterMode,
            static_cast<int>(this->pDetector->GetVar<XSPD::CounterMode>("counter_mode")));
        setIntegerParam(ADXSPD_SaturationFlag,
                        static_cast<int>(this->pDetector->GetVar<XSPD::OnOff>("saturation_flag")));
        setIntegerParam(
            ADXSPD_ShuffleMode,
            static_cast<int>(this->pDetector->GetVar<XSPD::ShuffleMode>("shuffle_mode")));

        setStringParam(ADModel, this->pDetector->GetVar<string>("type").c_str());

        vector<double> thresholds = this->pDetector->GetVar<vector<double>>("thresholds");
        if (thresholds.size() > 0) setDoubleParam(ADXSPD_LowThreshold, thresholds[0]);
        if (thresholds.size() > 1) setDoubleParam(ADXSPD_HighThreshold, thresholds[1]);

        int maxSizeX = this->pDetector->GetActiveDataPort()->GetVar<int>("frame_width");
        int maxSizeY = this->pDetector->GetActiveDataPort()->GetVar<int>("frame_height");
        setIntegerParam(ADMaxSizeX, maxSizeX);
        setIntegerParam(ADMaxSizeY, maxSizeY);
        setIntegerParam(ADSizeX, maxSizeX);
        setIntegerParam(ADSizeY, maxSizeY);

        setIntegerParam(ADXSPD_RoiRows, this->pDetector->GetVar<int>("roi_rows"));

        // TODO: BinY should reflect roi rows and vice versa.
        setIntegerParam(ADBinX, 1);
        setIntegerParam(ADBinY, 1);
        setIntegerParam(ADMinX, 0);
        setIntegerParam(ADMinY, 0);
        setIntegerParam(NDColorMode, NDColorModeMono);

    } catch (std::exception& e) {
        ERR_ARGS("Failed to get initial detector state: %s", e.what());
    }

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
        ERR_TO_STATUS("Cannot set parameter %s while acquiring", paramName);
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
                setIntegerParam(ADNumImages, this->pDetector->SetVar<int>("n_frames", 1));
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
            ERR_TO_STATUS("Invalid number of images: %d (valid range: 1-%d)", value, maxNumImages);
            return asynError;
        }
        setIntegerParam(ADNumImages, this->pDetector->SetVar<int>("n_frames", value));
        if (value == 1)
            setIntegerParam(ADImageMode, ADImageSingle);
        else
            setIntegerParam(ADImageMode, ADImageMultiple);
    } else if (function < ADXSPD_FIRST_PARAM) {
        status = ADDriver::writeInt32(pasynUser, value);
    } else {
        try {
            int actualValue = value;
            string endpoint;
            if (function == ADXSPD_BitDepth) {
                actualValue = this->pDetector->SetVar<int>("bit_depth", value);
                setIntegerParam(NDDataType, static_cast<int>(getDataTypeForBitDepth(actualValue)));
            } else if (function == ADXSPD_SummedFrames) {
                actualValue = this->pDetector->SetVar<int>("summed_frames", value);
            } else if (function == ADXSPD_RoiRows) {
                actualValue = this->pDetector->SetVar<int>("roi_rows", value);
            } else if (function == ADXSPD_GatingMode) {
                actualValue = static_cast<int>(this->pDetector->SetVar<XSPD::OnOff>(
                    "gating_mode", static_cast<XSPD::OnOff>(value)));
            } else if (function == ADXSPD_FFCorrection) {
                actualValue = static_cast<int>(this->pDetector->SetVar<XSPD::OnOff>(
                    "flatfield_correction", static_cast<XSPD::OnOff>(value)));
            } else if (function == ADXSPD_ChargeSumming) {
                actualValue = static_cast<int>(this->pDetector->SetVar<XSPD::OnOff>(
                    "charge_summing", static_cast<XSPD::OnOff>(value)));
            } else if (function == ADTriggerMode) {
                actualValue = static_cast<int>(this->pDetector->SetVar<XSPD::TriggerMode>(
                    "trigger_mode", static_cast<XSPD::TriggerMode>(value)));
            } else if (function == ADXSPD_CrCorr) {
                actualValue = static_cast<int>(this->pDetector->SetVar<XSPD::OnOff>(
                    "countrate_correction", static_cast<XSPD::OnOff>(value)));
            } else if (function == ADXSPD_CounterMode) {
                actualValue = static_cast<int>(this->pDetector->SetVar<XSPD::CounterMode>(
                    "counter_mode", static_cast<XSPD::CounterMode>(value)));
            } else if (function == ADXSPD_SaturationFlag) {
                actualValue = static_cast<int>(this->pDetector->SetVar<XSPD::OnOff>(
                    "saturation_flag", static_cast<XSPD::OnOff>(value)));
            } else if (function == ADXSPD_ShuffleMode) {
                actualValue = static_cast<int>(this->pDetector->SetVar<XSPD::ShuffleMode>(
                    "shuffle_mode", static_cast<XSPD::ShuffleMode>(value)));
            } else if (function == ADXSPD_StatusInterval) {
                if (value < ADXSPD_MIN_STATUS_POLL_INTERVAL) {
                    actualValue = ADXSPD_MIN_STATUS_POLL_INTERVAL;
                }
            }

            setIntegerParam(function, actualValue);
            if (actualValue != value) {
                WARN_ARGS("Requested value %d for parameter %s, but set value is %d", value,
                          paramName, actualValue);
                status = asynError;
            }
        } catch (std::invalid_argument& e) {
            ERR_ARGS("Invalid argument when setting parameter %s: %s", paramName, e.what());
            return asynError;
        } catch (std::runtime_error& e) {
            ERR_ARGS("Runtime error when setting parameter %s: %s", paramName, e.what());
            return asynError;
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
        ERR_TO_STATUS("Cannot set param %s while acquiring", paramName);
        return asynError;
    }

    if (function == ADAcquireTime) {
        actualValue = this->pDetector->SetVar<double>("shutter_time", value * 1000.0) /
                      1000.0;  // XSPD API uses milliseconds
        setDoubleParam(ADAcquireTime, actualValue);
    } else if (function < ADXSPD_FIRST_PARAM) {
        status = ADDriver::writeFloat64(pasynUser, value);
    } else {
        double actualValue = value;
        string endpoint;
        if (function == ADXSPD_BeamEnergy) {
            endpoint = "beam_energy";
            actualValue = this->pDetector->SetVar<double>(endpoint, value);
        } else if (function == ADXSPD_LowThreshold) {
            actualValue = this->pDetector->SetThreshold(XSPD::Threshold::LOW, value);
        } else if (function == ADXSPD_HighThreshold) {
            actualValue = this->pDetector->SetThreshold(XSPD::Threshold::HIGH, value);
        } else if (function == ADXSPD_StatusInterval && value < ADXSPD_MIN_STATUS_POLL_INTERVAL) {
            actualValue = ADXSPD_MIN_STATUS_POLL_INTERVAL;
        }
        setDoubleParam(function, actualValue);
        if (actualValue != value) {
            WARN_ARGS("Requested value %f for parameter %s, but set value is %f", value, paramName,
                      actualValue);
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
 * @param ip The IP address of the XSPD device (e.g. 192.168.1.100)
 * @param portNum The port number of the XSPD device (e.g. 8080)
 * @param deviceId The device ID of the XSPD device to connect to (if NULL, connects to first device
 * found)
 */
ADXSPD::ADXSPD(const char* portName, const char* ip, int portNum, const char* deviceId)
    : ADDriver(portName, 1, (int) NUM_ADXSPD_PARAMS, 0, 0, 0, 0, 0, 1, 0, 0) {
    // Create ADXSPD specific asyn parameters
    createAllParams();

    // Sets driver version PV (version numbers defined in header file)
    char versionString[25];
    snprintf(versionString, sizeof(versionString), "%d.%d.%d", ADXSPD_VERSION, ADXSPD_REVISION,
             ADXSPD_MODIFICATION);
    setStringParam(NDDriverVersion, versionString);

    INFO_ARGS("Connecting to XSPD api at %s:%d...", ip, portNum);
    this->pApi = new XSPD::API(string(ip), portNum);
    if (deviceId == nullptr) {
        INFO("No device ID specified, will connect to first device found.");
        this->pDetector = this->pApi->Initialize();
    } else {
        INFO_ARGS("Requested device ID: %s", deviceId);
        this->pDetector = this->pApi->Initialize(string(deviceId));
    }

    INFO_ARGS("Connected to detector w/ ID: %s", this->pDetector->GetId().c_str());

    this->zmqContext = zmq_ctx_new();

    setStringParam(ADXSPD_ApiVersion, this->pApi->GetApiVersion().c_str());
    setStringParam(ADXSPD_Version, this->pApi->GetXSPDVersion().c_str());
    setStringParam(ADSerialNumber, this->pApi->GetDeviceId().c_str());
    setStringParam(ADManufacturer, "X-Spectrum GmbH");
    setStringParam(ADModel, this->detectorId.c_str());
    setStringParam(ADSDKVersion, this->pApi->GetLibXSPVersion().c_str());
    setStringParam(ADFirmwareVersion, this->pDetector->GetFirmwareVersion().c_str());

    // Initialize our modules
    vector<XSPD::Module*> moduleList = this->pDetector->GetModules();
    int numModules = (int) moduleList.size();
    setIntegerParam(ADXSPD_NumModules, numModules);
    for (int index = 0; index < numModules; index++) {
        string modulePortName = string(portName) + "_MOD" + std::to_string(index + 1);
        this->modules.push_back(new ADXSPDModule(modulePortName.c_str(), moduleList[index], this));
    }

    this->getInitialDetState();

    // Create a shutdown event so we can signal to other threads to exit.
    this->shutdownEventId = epicsEventCreate(epicsEventEmpty);

    // Spawn our acquisition thread
    epicsThreadOpts opts;
    opts.priority = epicsThreadPriorityHigh;
    opts.stackSize = epicsThreadGetStackSize(epicsThreadStackBig);
    opts.joinable = 1;
    this->acquisitionThreadId = epicsThreadCreateOpt(
        "acquisitionThread", (EPICSTHREADFUNC) acquisitionThreadC, this, &opts);

    // Spawn our monitoring thread
    epicsThreadOpts monitorOpts;
    monitorOpts.priority = epicsThreadPriorityMedium;
    monitorOpts.stackSize = epicsThreadGetStackSize(epicsThreadStackMedium);
    monitorOpts.joinable = 1;
    this->monitorThreadId =
        epicsThreadCreateOpt("monitorThread", (EPICSTHREADFUNC) monitorThreadC, this, &monitorOpts);

    // when epics is exited, delete the instance of this class
    epicsAtExit(exitCallbackC, this);
}

/**
 * @brief Destructor for the ADXSPD driver
 */
ADXSPD::~ADXSPD() {
    INFO("Shutting down ADXSPD driver...");

    int acquiring;
    getIntegerParam(ADAcquire, &acquiring);
    if (acquiring) {
        INFO("Stopping acquisition...");
        acquireStop();
    }

    INFO("Signaling shutdown event...");
    epicsEventSignal(this->shutdownEventId);

    if (this->monitorThreadId != nullptr) {
        INFO("Waiting for monitoring thread to join...");
        epicsThreadMustJoin(this->monitorThreadId);
    }

    if (this->zmqContext != nullptr) {
        INFO("Destroying zmq context...");
        zmq_ctx_destroy(this->zmqContext);
    }

    if (this->acquisitionThreadId != nullptr) {
        INFO("Waiting for acquisition thread to join...");
        epicsThreadMustJoin(this->acquisitionThreadId);
    }

    for (auto& module : this->modules) {
        delete module;
    }

    INFO("Releasing detector and API objects...");
    delete this->pDetector;
    delete this->pApi;

    this->shutdownPortDriver();

    INFO("Done.");
}

//-------------------------------------------------------------
// ADXSPD ioc shell registration
//-------------------------------------------------------------

static const iocshArg XSPDConfigArg0 = {"Port name", iocshArgString};
static const iocshArg XSPDConfigArg1 = {"IP Address", iocshArgString};
static const iocshArg XSPDConfigArg2 = {"Port Number", iocshArgInt};
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
static void ADXSPDRegister(void) { iocshRegister(&configXSPD, configXSPDCallFunc); }

/* external function for IOC registration */
extern "C" {
epicsExportRegistrar(ADXSPDRegister);
}
