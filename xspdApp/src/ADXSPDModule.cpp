/*
 * ADXSPDModule.cpp
 *
 * This is the module class for the ADXSPD area detector driver.
 * It handles communication with a single module in an X-Spectrum
 * XSPD detector.
 *
 */

#include "ADXSPDModule.h"

<<<<<<< HEAD
ADXSPDModule::ADXSPDModule(const char* portName, const char* moduleId, ADXSPD* pPvt,
=======
template <typename T>
T ADXSPDModule::getModuleParam(string endpoint) {
    const char* functionName = "getModuleParam";
    string moduleEndpoint = "devices/" + parent->getDeviceId() +
                            "/variables?path=" + parent->getDetectorId() + "/" + moduleId;

    json response = parent->xspdGet(moduleEndpoint + "/" + endpoint)["value"];
    if (response.empty() || response.is_null()) {
        ERR_ARGS("Failed to get module parameter %s for module %s", endpoint.c_str(),
                 moduleId.c_str());
        return NULL;
    } else {
        return response.get<T>();
    }
}

void ADXSPDModule::checkStatus() {
    const char* functionName = "checkStatus";

    // Example: Read module temperature and update parameter
    setDoubleParam(ADXSPDModule_ModSensCurr, getModuleParam<double>("sensor_current"));

    vector<double> temps = getModuleParam<vector<double>>("temperature");
    setDoubleParam(ADXSPDModule_ModBoardTemp, temps[0]);
    setDoubleParam(ADXSPDModule_ModFpgaTemp, temps[1]);
    setDoubleParam(ADXSPDModule_ModHumTemp, temps[2]);

    setDoubleParam(ADXSPDModule_ModHum, getModuleParam<double>("humidity"));
    // Add more status checks as needed
}

ADXSPDModule::ADXSPDModule(const char* portName, const char* moduleId, ADXSPD* parent,
>>>>>>> 691678ff4018adf90d2bb07cbbf4b548fa66073e
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
    this->parent = parent;
    this->moduleId = string(moduleId);
    this->moduleIndex = moduleIndex;
    createAllParams();

    INFO_ARGS("Configured ADXSPDModule for module %s (index %d)", moduleId, moduleIndex);
}

ADXSPDModule::~ADXSPDModule() {
    const char* functionName = "~ADXSPDModule";
    INFO_ARGS("Destroying module %s", this->moduleId.c_str());
}
