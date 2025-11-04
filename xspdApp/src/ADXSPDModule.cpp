/*
 * ADXSPDModule.cpp
 *
 * This is the module class for the ADXSPD area detector driver.
 * It handles communication with a single module in an X-Spectrum
 * XSPD detector.
 *
 */

#include "ADXSPDModule.h"

template <typename T>
T ADXSPDModule::getModuleParam(string endpoint, string key) {
    const char* functionName = "getModuleParam";
    string moduleEndpoint = "devices/" + parent->getDeviceId() +
                            "/variables?path=" + parent->getDetectorId() + "/" + moduleId;

    json response = parent->xspdGet(moduleEndpoint + "/" + endpoint);
    if (response.empty() || response.is_null()) {
        ERR_ARGS("Failed to get module parameter %s for module %s", endpoint.c_str(),
                 moduleId.c_str());
        return T();
    } else if (!response.contains(key)) {
        ERR_ARGS("Key %s not found in module parameter %s for module %s", key.c_str(),
                 endpoint.c_str(), moduleId.c_str());
        return T();
    } else {
        return response[key].get<T>();
    }
}

template <typename T>
asynStatus ADXSPDModule::setModuleParam(string endpoint, T value) {
    const char* functionName = "setModuleParam";
    string moduleEndpoint = "devices/" + parent->getDeviceId() +
                            "/variables?path=" + parent->getDetectorId() + "/" + moduleId;

    asynStatus status = parent->xspdSet<T>(moduleEndpoint + "/" + endpoint, value);
    if (status != asynSuccess) {
        ERR_ARGS("Failed to set module parameter %s for module %s", endpoint.c_str(),
                 moduleId.c_str());
    }
    return status;
}

void ADXSPDModule::checkStatus() {
    const char* functionName = "checkStatus";

    // Example: Read module temperature and update parameter
    setDoubleParam(ADXSPDModule_ModSensCurr, getModuleParam<double>("sensor_current"));

    vector<double> temps = getModuleParam<vector<double>>("temperature");
    setDoubleParam(ADXSPDModule_ModBoardTemp, temps[0]);
    setDoubleParam(ADXSPDModule_ModFpgaTemp, temps[1]);
    setDoubleParam(ADXSPDModule_ModHumTemp, temps[2]);

    setDoubleParam(ADXSPDModule_ModHum, getModuleParam<double>("humidity"));
    // Add more status checks as needed

    

    callParamCallbacks();
}

void ADXSPDModule::getInitialState() {
    const char* functionName = "getInitialState";

    this->checkStatus();

    int compressionLevel = getModuleParam<int>("compression_level");
    setIntegerParam(ADXSPDModule_ModCompressLevel, compressionLevel);
    
    ADXSPD_Compressor_t compressor;
    string compStr = getModuleParam<string>("compressor");
    if (compStr == "none") {
        compressor = ADXSPD_COMPRESSOR_NONE;
    } else if (compStr == "zlib") {
        compressor = ADXSPD_COMPRESSOR_ZLIB;
    } else if (compStr == "bslz4") {
        compressor = ADXSPD_COMPRESSOR_BLOSC;
    } else {
        ERR_ARGS("Unknown compressor string '%s' for module %s", compStr.c_str(),
                 moduleId.c_str());
        compressor = ADXSPD_COMPRESSOR_NONE;
    }

    ADXSPD_OnOff_t flatfieldEnabled =
        parent->parseOnOff(getModuleParam<string>("flatfield_enabled"));
    setIntegerParam(ADXSPDModule_ModFlatfieldEnabled, flatfieldEnabled);

    string flatfieldStatus = getModuleParam<string>("flatfield_status");
    setStringParam(ADXSPDModule_ModFlatfieldStatus, flatfieldStatus.c_str());

    ADXSPD_OnOff_t interpolation =
        parent->parseOnOff(getModuleParam<string>("interpolation"));
    setIntegerParam(ADXSPDModule_ModInterpMode, interpolation);

    int nConnectors = getModuleParam<int>("n_connectors");
    setIntegerParam(ADXSPDModule_ModNumConnectors, nConnectors);

    int maxFrames = getModuleParam<int>("max_frames");
    setIntegerParam(ADXSPDModule_ModMaxFrames, maxFrames);

    vector<double> position = getModuleParam<vector<double>>("position");
    setDoubleParam(ADXSPDModule_ModPosX, position[0]);
    setDoubleParam(ADXSPDModule_ModPosY, position[1]);
    setDoubleParam(ADXSPDModule_ModPosZ, position[2]);
    
    ADXSPD_OnOff_t ramAllocated = 
        parent->parseOnOff(getModuleParam<string>("ram_allocated"));
    setIntegerParam(ADXSPDModule_ModRamAllocated, ramAllocated);

    vector<double> rotation = getModuleParam<vector<double>>("rotation");
    setDoubleParam(ADXSPDModule_ModRotX, rotation[0]);
    setDoubleParam(ADXSPDModule_ModRotY, rotation[1]);
    setDoubleParam(ADXSPDModule_ModRotZ, rotation[2]);

    int satThreshold = getModuleParam<int>("saturation_threshold");
    setIntegerParam(ADXSPDModule_ModSatThreshold, satThreshold);

    callParamCallbacks();


}

ADXSPDModule::ADXSPDModule(const char* portName, const char* moduleId, ADXSPD* parent,
                           int moduleIndex)
    : asynPortDriver(
          portName, 1, /* maxAddr */
          asynInt32Mask | asynFloat64Mask | asynFloat64ArrayMask | asynDrvUserMask |
              asynOctetMask, /* Interface mask */
          asynInt32Mask | asynFloat64Mask | asynFloat64ArrayMask |
              asynOctetMask, /* Interrupt mask */
          0, /* asynFlags.  This driver does not block and it is not multi-device, so flag is 0 */
          1, /* Autoconnect */
          0, /* Default priority */
          0) /* Default stack size*/
{
    static const char* functionName = "ADXSPDModule";
    this->parent = parent;
    this->moduleId = string(moduleId);
    this->moduleEndpoint = parent->getDetectorId() + "/" + this->moduleId;
    this->moduleIndex = moduleIndex;
    this->createAllParams();

    this->getInitialState();
    INFO_ARGS("Configured ADXSPDModule for module %s (index %d)", moduleId, moduleIndex);
}

ADXSPDModule::~ADXSPDModule() {
    const char* functionName = "~ADXSPDModule";
    INFO_ARGS("Destroying module %s", this->moduleId.c_str());
}
