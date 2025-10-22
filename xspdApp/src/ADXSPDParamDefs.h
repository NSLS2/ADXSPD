#ifndef ADXSPD_PARAM_DEFS_H
#define ADXSPD_PARAM_DEFS_H

// This file is auto-generated. Do not edit directly.
// Generated from ADXSPD.template

// String definitions for parameters
#define ADXSPD_ApiVersionString "XSPD_API_VERSION"
#define ADXSPD_VersionString "XSPD_VERSION"
#define ADXSPD_NumModulesString "XSPD_NUM_MODULES"
#define ADXSPD_CompressLevelString "XSPD_COMPRESS_LEVEL"
#define ADXSPD_BeamEnergyString "XSPD_BEAM_ENERGY"
#define ADXSPD_SaturationFlagString "XSPD_SATURATION_FLAG"
#define ADXSPD_ChargeSummingString "XSPD_CHARGE_SUMMING"
#define ADXSPD_FFCorrectionString "XSPD_F_F_CORRECTION"
#define ADXSPD_GatingModeString "XSPD_GATING_MODE"
#define ADXSPD_DetectorStateString "XSPD_DETECTOR_STATE"
#define ADXSPD_HighThresholdString "XSPD_HIGH_THRESHOLD"
#define ADXSPD_LowThresholdString "XSPD_LOW_THRESHOLD"
#define ADXSPD_TriggerModeString "XSPD_TRIGGER_MODE"
#define ADXSPD_CounterModeString "XSPD_COUNTER_MODE"
#define ADXSPD_ResetString "XSPD_RESET"
#define ADXSPD_GenerateFlatfieldString "XSPD_GENERATE_FLATFIELD"
#define ADXSPD_FlatfieldStatusString "XSPD_FLATFIELD_STATUS"
#define ADXSPD_ShuffleModeString "XSPD_SHUFFLE_MODE"

// Parameter index definitions
int ADXSPD_ApiVersion;
int ADXSPD_Version;
int ADXSPD_NumModules;
int ADXSPD_CompressLevel;
int ADXSPD_BeamEnergy;
int ADXSPD_SaturationFlag;
int ADXSPD_ChargeSumming;
int ADXSPD_FFCorrection;
int ADXSPD_GatingMode;
int ADXSPD_DetectorState;
int ADXSPD_HighThreshold;
int ADXSPD_LowThreshold;
int ADXSPD_TriggerMode;
int ADXSPD_CounterMode;
int ADXSPD_Reset;
int ADXSPD_GenerateFlatfield;
int ADXSPD_FlatfieldStatus;
int ADXSPD_ShuffleMode;

#define ADXSPD_FIRST_PARAM ADXSPD_ApiVersion
#define ADXSPD_LAST_PARAM ADXSPD_ShuffleMode

#define NUM_ADXSPD_PARAMS ((int) (&ADXSPD_LAST_PARAM - &ADXSPD_FIRST_PARAM + 1))

#endif
