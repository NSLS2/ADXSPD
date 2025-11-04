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
T ADXSPDModule::xspdGetModuleVar(string endpoint, string key) {
    const char* functionName = "xspdGetModuleVar";

    string fullVarEndpoint = this->moduleId + "/" + endpoint;

    return this->parent->xspdGetVar<T>(fullVarEndpoint, key);
}

void ADXSPDModule::checkStatus() {
    const char* functionName = "checkStatus";

    // Example: Read module temperature and update parameter
    setDoubleParam(ADXSPDModule_SensCurr, getModuleParam<double>("sensor_current"));

    vector<double> temps = getModuleParam<vector<double>>("temperature");
    setDoubleParam(ADXSPDModule_BoardTemp, temps[0]);
    setDoubleParam(ADXSPDModule_FpgaTemp, temps[1]);
    setDoubleParam(ADXSPDModule_HumTemp, temps[2]);

    setDoubleParam(ADXSPDModule_Hum, getModuleParam<double>("humidity"));
    // Add more status checks as needed

    

    callParamCallbacks();
}

void ADXSPDModule::getInitialModuleState() {
    const char* functionName = "getInitialModuleState";

    this->checkStatus();

    // Compression settings
    setIntegerParam(ADXSPDModule_CompressLevel, xspdGetModuleVar<int>("compression_level"));
    setIntegerParam(ADXSPDModule_Compressor, (int) xspdGetModuleVar<ADXSPDCompressor>("compressor"));

    setStringParam(ADXSPDModule_FfStatus, xspdGetModuleVar<string>("flatfield_status").c_str());
    setIntegerParam(ADXSPDModule_InterpMode, (int) xspdGetModuleVar<ADXSPDOnOff>("interpolation"));

    setIntegerParam(ADXSPDModule_NumCons, xspdGetModuleVar<int>("num_connectors"));

    setIntegerParam(ADXSPDModule_MaxFrames, xspdGetModuleVar<int>("max_frames"));

    vector<double> position = xspdGetModuleVar<vector<double>>("position");
    setDoubleParam(ADXSPDModule_PosX, position[0]);
    setDoubleParam(ADXSPDModule_PosY, position[1]);
    setDoubleParam(ADXSPDModule_PosZ, position[2]);
    
    setIntegerParam(ADXSPDModule_RamAllocated, xspdGetModuleVar<int>("ram_allocated"));

    vector<double> rotation = xspdGetModuleVar<vector<double>>("rotation");
    setDoubleParam(ADXSPDModule_RotYaw, rotation[0]);
    setDoubleParam(ADXSPDModule_RotPitch, rotation[1]);
    setDoubleParam(ADXSPDModule_RotRoll, rotation[2]);

    setIntegerParam(ADXSPDModule_SatThresh, xspdGetModuleVar<int>("saturation_threshold"));

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
