// This file is auto-generated. Do not edit directly.
// Generated from ADXSPDModule.template

#include "ADXSPDModule.h"

void ADXSPDModule::createAllParams() {
    createParam(ADXSPDModule_BoardTempString, asynParamFloat64, &ADXSPDModule_BoardTemp);
    createParam(ADXSPDModule_FpgaTempString, asynParamFloat64, &ADXSPDModule_FpgaTemp);
    createParam(ADXSPDModule_HumTempString, asynParamFloat64, &ADXSPDModule_HumTemp);
    createParam(ADXSPDModule_HumString, asynParamFloat64, &ADXSPDModule_Hum);
    createParam(ADXSPDModule_VoltageString, asynParamFloat64, &ADXSPDModule_Voltage);
    createParam(ADXSPDModule_SensCurrString, asynParamFloat64, &ADXSPDModule_SensCurr);
    createParam(ADXSPDModule_SatThreshString, asynParamInt32, &ADXSPDModule_SatThresh);
    createParam(ADXSPDModule_RotYawString, asynParamFloat64, &ADXSPDModule_RotYaw);
    createParam(ADXSPDModule_RotPitchString, asynParamFloat64, &ADXSPDModule_RotPitch);
    createParam(ADXSPDModule_RotRollString, asynParamFloat64, &ADXSPDModule_RotRoll);
    createParam(ADXSPDModule_PosXString, asynParamFloat64, &ADXSPDModule_PosX);
    createParam(ADXSPDModule_PosYString, asynParamFloat64, &ADXSPDModule_PosY);
    createParam(ADXSPDModule_PosZString, asynParamFloat64, &ADXSPDModule_PosZ);
    createParam(ADXSPDModule_MaxFramesString, asynParamInt32, &ADXSPDModule_MaxFrames);
    createParam(ADXSPDModule_NumSubframesString, asynParamInt32, &ADXSPDModule_NumSubframes);
    createParam(ADXSPDModule_NumConsString, asynParamInt32, &ADXSPDModule_NumCons);
    createParam(ADXSPDModule_InterpModeString, asynParamInt32, &ADXSPDModule_InterpMode);
    createParam(ADXSPDModule_FeatBitmaskString, asynParamInt32, &ADXSPDModule_FeatBitmask);
    createParam(ADXSPDModule_CompressLevelString, asynParamInt32, &ADXSPDModule_CompressLevel);
    createParam(ADXSPDModule_CompressorString, asynParamInt32, &ADXSPDModule_Compressor);
    createParam(ADXSPDModule_RamAllocatedString, asynParamInt32, &ADXSPDModule_RamAllocated);
    createParam(ADXSPDModule_FfEnabledString, asynParamInt32, &ADXSPDModule_FfEnabled);
    createParam(ADXSPDModule_FfStatusString, asynParamOctet, &ADXSPDModule_FfStatus);
    createParam(ADXSPDModule_LowThreshFfAuthorString, asynParamOctet,
                &ADXSPDModule_LowThreshFfAuthor);
    createParam(ADXSPDModule_HighThreshFfAuthorString, asynParamOctet,
                &ADXSPDModule_HighThreshFfAuthor);
    createParam(ADXSPDModule_LowThreshFfErrString, asynParamOctet, &ADXSPDModule_LowThreshFfErr);
    createParam(ADXSPDModule_HighThreshFfErrString, asynParamOctet, &ADXSPDModule_HighThreshFfErr);
    createParam(ADXSPDModule_LowThreshFfDateString, asynParamOctet, &ADXSPDModule_LowThreshFfDate);
    createParam(ADXSPDModule_HighThreshFfDateString, asynParamOctet,
                &ADXSPDModule_HighThreshFfDate);
    createParam(ADXSPDModule_FramesQueuedString, asynParamInt32, &ADXSPDModule_FramesQueued);
    createParam(ADXSPDModule_PixelMaskString, asynParamOctet, &ADXSPDModule_PixelMask);
    createParam(ADXSPDModule_UsePixelMaskString, asynParamInt32, &ADXSPDModule_UsePixelMask);
    createParam(ADXSPDModule_ShuffleModeString, asynParamInt32, &ADXSPDModule_ShuffleMode);
}
