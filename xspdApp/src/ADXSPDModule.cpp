/*
 * ADXSPDModule.cpp
 *
 * This is the module class for the ADXSPD area detector driver.
 * It handles communication with a single module in an X-Spectrum
 * XSPD detector.
 *
 */

#include "ADXSPDModule.h"

ADXSPDModule::ADXSPDModule(const char* portName, const char* moduleId, ADXSPD* pPvt,
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
    this->parent = pPvt;
    this->moduleId = string(moduleId);
    this->moduleIndex = moduleIndex;
    createAllParams();
}

ADXSPDModule::~ADXSPDModule() {
    const char* functionName = "~ADXSPDModule";
    INFO_ARGS("Destroying module %s", this->moduleId.c_str());
}
