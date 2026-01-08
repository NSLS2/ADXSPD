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

class ADXSPDModule : public asynPortDriver {
   public:
    ADXSPDModule(const char* portName, XSPD::Module* module, ADXSPD* parent);
    ~ADXSPDModule();

    // These are the methods that we override from asynPortDriver
    // virtual asynStatus writeInt32(asynUser* pasynUser, epicsInt32 value);
    // virtual asynStatus writeFloat64(asynUser* pasynUser, epicsFloat64 value);
    // virtual void report(FILE* fp, int details);

    void checkStatus();
    void getInitialModuleState();
    int getMaxNumImages();

   protected:
    // Module parameters
#include "ADXSPDModuleParamDefs.h"

   private:
    const char* driverName = "ADXSPDModule";
    ADXSPD* parent;        // Pointer to the parent ADXSPD driver object
    XSPD::Module* module;  // Pointer to the XSPD module object
    void createAllParams();

    ADXSPDLogLevel getLogLevel() { return this->parent->getLogLevel(); }
};

#endif
