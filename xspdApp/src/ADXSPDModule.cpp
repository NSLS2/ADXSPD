/*
 * ADXSPDModule.cpp
 *
 * This is the module class for the ADXSPD area detector driver.
 * It handles communication with a single module in an X-Spectrum
 * XSPD detector.
 *
 */

#include "ADXSPDModule.h"

void ADXSPDModule::checkStatus() {
    // Example: Read module temperature and update parameter
    setDoubleParam(ADXSPDModule_SensCurr, this->module->GetVar<double>("sensor_current"));

    vector<double> temps = this->module->GetVar<vector<double>>("temperature");
    setDoubleParam(ADXSPDModule_BoardTemp, temps[0]);
    setDoubleParam(ADXSPDModule_FpgaTemp, temps[1]);
    setDoubleParam(ADXSPDModule_HumTemp, temps[2]);

    setDoubleParam(ADXSPDModule_Hum, this->module->GetVar<double>("humidity"));
    // Add more status checks as needed

    callParamCallbacks();
}

void ADXSPDModule::getInitialModuleState() {
    this->checkStatus();

    // Compression settings
    setIntegerParam(ADXSPDModule_CompressLevel, this->module->GetVar<int>("compression_level"));
    setIntegerParam(ADXSPDModule_Compressor,
                    (int) this->module->GetVar<XSPD::Compressor>("compressor"));

    setStringParam(ADXSPDModule_FfStatus, this->module->GetVar<string>("flatfield_status").c_str());
    setIntegerParam(ADXSPDModule_InterpMode,
                    (int) this->module->GetVar<XSPD::OnOff>("interpolation"));

    setIntegerParam(ADXSPDModule_NumCons, this->module->GetVar<int>("n_connectors"));

    setIntegerParam(ADXSPDModule_MaxFrames, this->module->GetVar<int>("max_frames"));

    vector<double> position = this->module->GetVar<vector<double>>("position");
    setDoubleParam(ADXSPDModule_PosX, position[0]);
    setDoubleParam(ADXSPDModule_PosY, position[1]);
    setDoubleParam(ADXSPDModule_PosZ, position[2]);

    // TODO: ram_allocated endpoint doesn't seem to work
    // setIntegerParam(ADXSPDModule_RamAllocated, this->module->GetVar<int>("ram_allocated"));

    vector<double> rotation = this->module->GetVar<vector<double>>("rotation");
    setDoubleParam(ADXSPDModule_RotYaw, rotation[0]);
    setDoubleParam(ADXSPDModule_RotPitch, rotation[1]);
    setDoubleParam(ADXSPDModule_RotRoll, rotation[2]);

    setIntegerParam(ADXSPDModule_SatThresh, this->module->GetVar<int>("saturation_threshold"));

    // vector<string> features = this->module->GetVar<vector<string>>("features");
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
    int numImages = this->module->GetVar<int>("max_frames");
    setIntegerParam(ADXSPDModule_MaxFrames, numImages);
    callParamCallbacks();
    return numImages;
}

ADXSPDModule::ADXSPDModule(const char* portName, XSPD::Module* module, ADXSPD* parent)
    : asynPortDriver(
          portName, 1, /* maxAddr */
          asynInt32Mask | asynFloat64Mask | asynFloat64ArrayMask | asynDrvUserMask |
              asynOctetMask, /* Interface mask */
          asynInt32Mask | asynFloat64Mask | asynFloat64ArrayMask |
              asynOctetMask, /* Interrupt mask */
          0, /* asynFlags.  This driver does not block and it is not multi-device, so flag is 0 */
          1, /* Autoconnect */
          0, /* Default priority */
          0),
      parent(parent),
      module(module) /* Default stack size*/
{
    this->createAllParams();

    this->getInitialModuleState();
    INFO_ARGS("Configured ADXSPDModule w/ port %s for module %s", portName,
              module->GetId().c_str());
}

ADXSPDModule::~ADXSPDModule() {
    INFO_ARGS("Destroying module %s", this->module->GetId().c_str());
    this->shutdownPortDriver();
}
