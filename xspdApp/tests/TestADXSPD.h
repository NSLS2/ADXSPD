#ifndef TEST_ADXSPD_H
#define TEST_ADXSPD_H

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdlib>

#include "ADXSPD.h"
#include "cpr/cpr.h"

using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrictMock;

/**
 * XSPDService.h
 *
 * Header file for XSPD service test utilities.
 *  Author: Jakub Wlodek
 */

class TestADXSPD : public ::testing::Test {
   protected:
    void SetUpTestSuite() {
        // this->simulatedXSPDService = boost::process::child(".pixi/envs/default/bin/python3
        // sim/xspdSimulator.py",
        //                                    boost::process::std_out > boost::process::null,
        //                                    boost::process::std_err > boost::process::null);
        // // Give the simulated service some time to start up

        //     epicsThreadSleep(0.25);
    }

    void SetUp() override {
        std::cout << "Capturing output for test." << std::endl;
        ::testing::internal::CaptureStdout();
        ::testing::internal::CaptureStderr();
        // this->xspdDriver = new ADXSPD(this->getUniquePortName().c_str(),
        // "http://localhost:8000");
    }

    void TearDown() override {
        // Code here will be called immediately after each test (right
        // before the destructor).
        // delete this->xspdDriver;
        // cpr::Response r = cpr::Post(cpr::Url{"http://localhost:8000/simreset"});
    }

    void TearDownTestSuite() {
        // this->simulatedXSPDService.terminate();
    }

    std::string getUniquePortName() {
        static int portCounter = 0;
        return "XSPD" + std::to_string(portCounter++);
    }

    ADXSPD* xspdDriver;

    //     template<typename T>
    //     asynStatus checkSignalWrite(const char* paramName, T value, T expectedValue) {
    //         int index;
    //         int status = xspdDriver->findParam(paramName, &index);
    //         if (status != asynSuccess) {
    //             ADD_FAILURE() << "Parameter not found: " << paramName;
    //       return asynError;
    //     }

    //     // Create minimal asynUser - only reason field needed for driver logic
    //     asynUser user = {};   // Zero-initialize all fields
    //     user.reason = index;  // This is what the driver checks to determine action

    //     // asyn calls writeInt32 after locking the driver
    //     xspdDriver->lock();
    //     if(constexpr (std::is_same<T, int>::value)) {
    //       status |= xspdDriver->writeInt32(&user, value);
    //     } else if constexpr (std::is_same<T, double>::value) {
    //       status |= xspdDriver->writeFloat64(&user, value);
    //     } else {
    //       ADD_FAILURE() << "Unsupported parameter type for param " << paramName;
    //       xspdDriver->unlock();
    //       status |= asynError;
    //     }
    //     xspdDriver->unlock();

    //     int actualValue;
    //     xspdDriver->getIntegerParam(index, &actualValue);

    //     if (actualValue != expectedValue) {
    //       ADD_FAILURE() << "writeInt32 did not set expected value for param " << paramName
    //                     << ": expected " << expectedValue << ", got " << actualValue;
    //     }

    //     return (asynStatus)status;
    //   }
};

#endif  // ADXSPD_TESTS_XSPD_SERVICE_H
