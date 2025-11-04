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

    string fullVarEndpoint = this->moduleId + "/" + endpoint;
    return this->parent->xspdGetVar<T>(fullVarEndpoint, key);
}

template <typename T>
T ADXSPDModule::xspdGetModuleEnumVar(string endpoint, string key) {

    string resp = xspdGetModuleVar<string>(endpoint, key);
    if(resp.empty()) {
        ERR_ARGS("Failed to get module enum variable %s", endpoint.c_str());
        return T(0);
    }

    auto enumValue = magic_enum::enum_cast<T>(resp, magic_enum::case_insensitive);
    if (enumValue.has_value()) {
        return enumValue.value();
    } else {
        ERR_ARGS("Failed to cast value %s to enum for variable %s",
                 resp.c_str(), endpoint.c_str());
        return T(0);
    }
}

void ADXSPDModule::checkStatus() {

    // Example: Read module temperature and update parameter
    setDoubleParam(ADXSPDModule_SensCurr, xspdGetModuleVar<double>("sensor_current"));

    vector<double> temps = xspdGetModuleVar<vector<double>>("temperature");
    setDoubleParam(ADXSPDModule_BoardTemp, temps[0]);
    setDoubleParam(ADXSPDModule_FpgaTemp, temps[1]);
    setDoubleParam(ADXSPDModule_HumTemp, temps[2]);

    setDoubleParam(ADXSPDModule_Hum, xspdGetModuleVar<double>("humidity"));
    // Add more status checks as needed

    callParamCallbacks();
}

void ADXSPDModule::getInitialModuleState() {

    this->checkStatus();

    // Compression settings
    setIntegerParam(ADXSPDModule_CompressLevel, xspdGetModuleVar<int>("compression_level"));
    setIntegerParam(ADXSPDModule_Compressor, (int) xspdGetModuleEnumVar<ADXSPDCompressor>("compressor"));

    setStringParam(ADXSPDModule_FfStatus, xspdGetModuleVar<string>("flatfield_status").c_str());
    setIntegerParam(ADXSPDModule_InterpMode, (int) xspdGetModuleEnumVar<ADXSPDOnOff>("interpolation"));

    setIntegerParam(ADXSPDModule_NumCons, xspdGetModuleVar<int>("n_connectors"));

    setIntegerParam(ADXSPDModule_MaxFrames, xspdGetModuleVar<int>("max_frames"));

    vector<double> position = xspdGetModuleVar<vector<double>>("position");
    setDoubleParam(ADXSPDModule_PosX, position[0]);
    setDoubleParam(ADXSPDModule_PosY, position[1]);
    setDoubleParam(ADXSPDModule_PosZ, position[2]);

    // TODO: ram_allocated endpoint doesn't seem to work
    // setIntegerParam(ADXSPDModule_RamAllocated, xspdGetModuleVar<int>("ram_allocated"));

    vector<double> rotation = xspdGetModuleVar<vector<double>>("rotation");
    setDoubleParam(ADXSPDModule_RotYaw, rotation[0]);
    setDoubleParam(ADXSPDModule_RotPitch, rotation[1]);
    setDoubleParam(ADXSPDModule_RotRoll, rotation[2]);

    setIntegerParam(ADXSPDModule_SatThresh, xspdGetModuleVar<int>("saturation_threshold"));

    // vector<string> features = xspdGetModuleVar<vector<string>>("features");
    // for(auto& featureStr : features) {
    //     auto feature = magic_enum::enum_cast<ADXSPDModuleFeature>("FEAT_" + featureStr);
    //     if (!feature.has_value()) {
    //         ERR_ARGS("Unknown module feature: %s", featureStr.c_str());
    //         continue;
    //     }
    //     // TODO: Make these into a bitmask parameter?
    //     switch(feature.value()) {
    //         case ADXSPDModuleFeature::FEAT_HV:
    //             setIntegerParam(ADXSPDModule_HvSup,1);
    //             break;
    //         case ADXSPDModuleFeature::FEAT_1_6_BIT:
    //             setIntegerParam(ADXSPDModule_16bitSup,1);
    //             break;
    //         case ADXSPDModuleFeature::FEAT_MEDIPIX_DAC_IO:
    //             setIntegerParam(ADXSPDModule_MpixDacIoSup,1);
    //             break;
    //         case ADXSPDModuleFeature::FEAT_EXTENDED_GATING:
    //             setIntegerParam(ADXSPDModule_ExtGatingSup,1);
    //             break;
    //     }
    // }

    callParamCallbacks();

}

int ADXSPDModule::getMaxNumImages() {
    int numImages = xspdGetModuleVar<int>("max_frames");
    setIntegerParam(ADXSPDModule_MaxFrames, numImages);
    callParamCallbacks();
    return numImages;
}

ADXSPDModule::ADXSPDModule(const char* portName, string moduleId, ADXSPD* parent)
    : asynPortDriver(
          portName, 1, /* maxAddr */
          asynInt32Mask | asynFloat64Mask | asynFloat64ArrayMask | asynDrvUserMask |
              asynOctetMask, /* Interface mask */
          asynInt32Mask | asynFloat64Mask | asynFloat64ArrayMask |
              asynOctetMask, /* Interrupt mask */
          0, /* asynFlags.  This driver does not block and it is not multi-device, so flag is 0 */
          1, /* Autoconnect */
          0, /* Default priority */
          0), parent(parent), moduleId(moduleId) /* Default stack size*/
{
    this->createAllParams();

    this->getInitialModuleState();
    INFO_ARGS("Configured ADXSPDModule w/ port %s for module %s", portName, moduleId.c_str());
}

ADXSPDModule::~ADXSPDModule() {
    INFO_ARGS("Destroying module %s", this->moduleId.c_str());
}
