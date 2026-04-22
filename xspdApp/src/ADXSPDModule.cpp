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
    // Module status/health checks
    setDoubleParam(ADXSPDModule_SensCurr, this->module->GetVar<double>("sensor_current"));

    vector<double> temps = this->module->GetVar<vector<double>>("temperature");
    setDoubleParam(ADXSPDModule_BoardTemp, temps[0]);
    setDoubleParam(ADXSPDModule_FpgaTemp, temps[1]);
    setDoubleParam(ADXSPDModule_HumTemp, temps[2]);

    setDoubleParam(ADXSPDModule_Hum, this->module->GetVar<double>("humidity"));

    // Module flatfield state
    setStringParam(ADXSPDModule_FfStatus, this->module->GetVar<string>("flatfield_status").c_str());

    // Module readout check
    setIntegerParam(ADXSPDModule_FramesQueued, this->module->GetVar<int>("frames_queued"));
    getMaxNumImages();
}

void ADXSPDModule::getFlatfieldState() {
    setIntegerParam(ADXSPDModule_FfEnabled,
                    this->module->GetVar<bool>("flatfield_enabled") ? 1 : 0);

    vector<string> ffTimestamps = this->module->GetVar<vector<string>>("flatfield_timestamp");
    setStringParam(ADXSPDModule_LowThreshFfDate, ffTimestamps[0].c_str());
    setStringParam(ADXSPDModule_HighThreshFfDate, ffTimestamps[1].c_str());

    vector<string> ffAuthors = this->module->GetVar<vector<string>>("flatfield_author");
    setStringParam(ADXSPDModule_LowThreshFfAuthor, ffAuthors[0].c_str());
    setStringParam(ADXSPDModule_HighThreshFfAuthor, ffAuthors[1].c_str());

    callParamCallbacks();
}

int ADXSPDModule::getMaxNumImages() {
    int maxFrames = this->module->GetVar<int>("max_frames");
    setIntegerParam(ADXSPDModule_MaxFrames, maxFrames);
    callParamCallbacks();
    return maxFrames;
}

void ADXSPDModule::getInitialModuleState() {
    this->checkStatus();

    // Compression settings
    setIntegerParam(ADXSPDModule_CompressLevel, this->module->GetVar<int>("compression_level"));
    setIntegerParam(ADXSPDModule_Compressor,
                    (int) this->module->GetVar<XSPD::Compressor>("compressor"));

    setIntegerParam(ADXSPDModule_InterpMode,
                    (int) this->module->GetVar<bool>("interpolation_enabled"));

    setIntegerParam(ADXSPDModule_NumCons, this->module->GetVar<int>("n_connectors"));

    setIntegerParam(ADXSPDModule_PixelMask, this->module->GetVar<bool>("pixel_mask_enabled"));

    getFlatfieldState();

    vector<double> position = this->module->GetVar<vector<double>>("position");
    setDoubleParam(ADXSPDModule_PosX, position[0]);
    setDoubleParam(ADXSPDModule_PosY, position[1]);
    setDoubleParam(ADXSPDModule_PosZ, position[2]);

    setIntegerParam(ADXSPDModule_RamAllocated, this->module->GetVar<bool>("ram_allocated"));

    vector<double> rotation = this->module->GetVar<vector<double>>("rotation");
    setDoubleParam(ADXSPDModule_RotYaw, rotation[0]);
    setDoubleParam(ADXSPDModule_RotPitch, rotation[1]);
    setDoubleParam(ADXSPDModule_RotRoll, rotation[2]);

    setIntegerParam(ADXSPDModule_SatThresh, this->module->GetVar<int>("saturation_threshold"));

    // Module feature support is stored as a bitmask w/ 4 bits.
    int featureBitmask = 0;
    for (auto& feature : this->module->GetFeatures()) {
        featureBitmask |= (1 << static_cast<int>(feature));
    }
    setUIntDigitalParam(ADXSPDModule_FeatBitmask, featureBitmask,
                        0x1F);  // 0x1F = 00011111, since we have 5 features in the enum

    setDoubleParam(ADXSPDModule_Voltage, this->module->GetVar<double>("voltage"));
    setIntegerParam(ADXSPDModule_NumSubframes, this->module->GetVar<int>("n_subframes"));

    callParamCallbacks();
}

ADXSPDModule::ADXSPDModule(const char* portName, XSPD::Module* module, ADXSPD* parent)
    : asynPortDriver(
          portName, 1, /* maxAddr */
          asynInt32Mask | asynUInt32DigitalMask | asynFloat64Mask | asynFloat64ArrayMask |
              asynDrvUserMask | asynOctetMask, /* Interface mask */
          asynInt32Mask | asynUInt32DigitalMask | asynFloat64Mask | asynFloat64ArrayMask |
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
