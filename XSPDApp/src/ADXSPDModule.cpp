/*
 * ADXSPDModule.cpp
 *
 * This is the module class for the ADXSPD area detector driver.
 * It handles communication with a single module in an X-Spectrum
 * XSPD detector.
 *
 */

#include "ADXSPDModule.h"

// Error message formatters
#define ERR(msg)                                               \
    if (this->parent->getLogLevel() >= ADXSPD_LOG_LEVEL_ERROR) \
        printf("ERROR | %s::%s: %s\n", driverName, functionName, msg);

#define ERR_ARGS(fmt, ...)                                     \
    if (this->parent->getLogLevel() >= ADXSPD_LOG_LEVEL_ERROR) \
        printf("ERROR | %s::%s: " fmt "\n", driverName, functionName, __VA_ARGS__);

// Warning message formatters
#define WARN(msg)                                                \
    if (this->parent->getLogLevel() >= ADXSPD_LOG_LEVEL_WARNING) \
        printf("WARNING | %s::%s: %s\n", driverName, functionName, msg);

#define WARN_ARGS(fmt, ...)                                      \
    if (this->parent->getLogLevel() >= ADXSPD_LOG_LEVEL_WARNING) \
        printf("WARNING | %s::%s: " fmt "\n", driverName, functionName, __VA_ARGS__);

// Info message formatters. Because there is no ASYN trace for info messages, we just use `printf`
// here.
#define INFO(msg)                                             \
    if (this->parent->getLogLevel() >= ADXSPD_LOG_LEVEL_INFO) \
        printf("INFO | %s::%s: %s\n", driverName, functionName, msg);

#define INFO_ARGS(fmt, ...)                                   \
    if (this->parent->getLogLevel() >= ADXSPD_LOG_LEVEL_INFO) \
        printf("INFO | %s::%s: " fmt "\n", driverName, functionName, __VA_ARGS__);

// Debug message formatters
#define DEBUG(msg)                                             \
    if (this->parent->getLogLevel() >= ADXSPD_LOG_LEVEL_DEBUG) \
    printf("DEBUG | %s::%s: %s\n", driverName, functionName, msg)

#define DEBUG_ARGS(fmt, ...)                                   \
    if (this->parent->getLogLevel() >= ADXSPD_LOG_LEVEL_DEBUG) \
        printf("DEBUG | %s::%s: %s" fmt "\n", driverName, functionName, __VA_ARGS__);

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
