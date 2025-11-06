// This file is auto-generated. Do not edit directly.
// Generated from ADXSPD.template

#include "ADXSPD.h"

void ADXSPD::createAllParams() {
    createParam(ADXSPD_ApiVersionString, asynParamOctet, &ADXSPD_ApiVersion);
    createParam(ADXSPD_VersionString, asynParamOctet, &ADXSPD_Version);
    createParam(ADXSPD_NumModulesString, asynParamInt32, &ADXSPD_NumModules);
    createParam(ADXSPD_CompressLevelString, asynParamInt32, &ADXSPD_CompressLevel);
    createParam(ADXSPD_CompressorString, asynParamInt32, &ADXSPD_Compressor);
    createParam(ADXSPD_BeamEnergyString, asynParamFloat64, &ADXSPD_BeamEnergy);
    createParam(ADXSPD_SaturationFlagString, asynParamInt32, &ADXSPD_SaturationFlag);
    createParam(ADXSPD_ChargeSummingString, asynParamInt32, &ADXSPD_ChargeSumming);
    createParam(ADXSPD_FFCorrectionString, asynParamInt32, &ADXSPD_FFCorrection);
    createParam(ADXSPD_GatingModeString, asynParamInt32, &ADXSPD_GatingMode);
    createParam(ADXSPD_TriggerModeString, asynParamInt32, &ADXSPD_TriggerMode);
    createParam(ADXSPD_CounterModeString, asynParamInt32, &ADXSPD_CounterMode);
    createParam(ADXSPD_ResetString, asynParamInt32, &ADXSPD_Reset);
    createParam(ADXSPD_GenerateFlatfieldString, asynParamInt32, &ADXSPD_GenerateFlatfield);
    createParam(ADXSPD_FlatfieldStatusString, asynParamOctet, &ADXSPD_FlatfieldStatus);
    createParam(ADXSPD_ShuffleModeString, asynParamInt32, &ADXSPD_ShuffleMode);
    createParam(ADXSPD_SummedFramesString, asynParamInt32, &ADXSPD_SummedFrames);
    createParam(ADXSPD_RoiRowsString, asynParamInt32, &ADXSPD_RoiRows);
    createParam(ADXSPD_LowThresholdString, asynParamFloat64, &ADXSPD_LowThreshold);
    createParam(ADXSPD_HighThresholdString, asynParamFloat64, &ADXSPD_HighThreshold);
    createParam(ADXSPD_FramesQueuedString, asynParamInt32, &ADXSPD_FramesQueued);
    createParam(ADXSPD_PixelMaskString, asynParamOctet, &ADXSPD_PixelMask);
    createParam(ADXSPD_GenFlatfieldString, asynParamInt32, &ADXSPD_GenFlatfield);
    createParam(ADXSPD_ResetDetectorString, asynParamInt32, &ADXSPD_ResetDetector);
    createParam(ADXSPD_BitDepthString, asynParamInt32, &ADXSPD_BitDepth);
    createParam(ADXSPD_CrCorrString, asynParamInt32, &ADXSPD_CrCorr);
}
