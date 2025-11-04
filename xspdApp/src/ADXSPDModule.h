#ifndef ADXSPD_MODULE_H
#define ADXSPD_MODULE_H
/*
 * Header file for the ADXSPDModule class
 *
 * Author: Jakub Wlodek
 * Copyright (c) : Brookhaven National Laboratory, 2025
 *
 */

#include <asynPortDriver.h>

#include "ADXSPD.h"

using namespace std;

enum class ADXSPDModuleFeature {
    FEAT_HV = 0,
    FEAT_1_6_BIT = 1,
    FEAT_MEDIPIX_DAC_IO = 2,
    FEAT_EXTENDED_GATING = 3,
};

class ADXSPDModule : public asynPortDriver {
   public:
    ADXSPDModule(const char* portName, string moduleId, ADXSPD* parent);
    ~ADXSPDModule();

    // These are the methods that we override from asynPortDriver
    // virtual asynStatus writeInt32(asynUser* pasynUser, epicsInt32 value);
    // virtual asynStatus writeFloat64(asynUser* pasynUser, epicsFloat64 value);
    // virtual void report(FILE* fp, int details);

    template <typename T>
    T xspdGetModuleVar(string endpoint, string key = "value");

    template <typename T>
    T xspdGetModuleEnumVar(string endpoint, string key = "value");

    template <typename T>
    asynStatus xspdSetModuleVar(string endpoint, T value);

    void checkStatus();
    void getInitialModuleState();

   protected:
    // Module parameters
#include "ADXSPDModuleParamDefs.h"

   private:
    const char* driverName = "ADXSPDModule";
    ADXSPD* parent;        // Pointer to the parent ADXSPD driver object
    string moduleId;  // Module ID string
    void createAllParams();

    ADXSPDLogLevel getLogLevel() { return this->parent->getLogLevel(); }
};

#endif
