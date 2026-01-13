#ifndef TEST_XSPDAPI_H
#define TEST_XSPDAPI_H

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdlib>

#include "MockXSPDAPI.h"
#include "cpr/cpr.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
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

class TestXSPDAPI : public ::testing::Test {
   protected:
    void SetUp() override {
        mockXSPDAPI = new StrictMock<MockXSPDAPI>();
        EXPECT_CALL(*mockXSPDAPI, GetApiVersion()).WillRepeatedly(Return("1"));
    }

    void TearDown() override { delete mockXSPDAPI; }

    // void mockConnectionTeardown(){
    //     // Any necessary teardown steps
    // }

    // void mockConnectionLifecycle(){
    //     mockConnectionSetup();
    //     // Code that uses the mock connection
    //     mockConnectionTeardown();
    // }

    StrictMock<MockXSPDAPI>* mockXSPDAPI;

    void MockGetRequest(string uri, json response) {
        EXPECT_CALL(*mockXSPDAPI, SubmitRequest(uri, XSPD::RequestType::GET))
            .WillOnce(Return(response));
        std::cout << "Mocked GET request to URI: " << uri << std::endl;
        std::cout << "Returning response: " << response.dump(4) << std::endl;
    }

    void MockRepeatedGetRequest(string uri, json response) {
        EXPECT_CALL(*mockXSPDAPI, SubmitRequest(uri, XSPD::RequestType::GET))
            .WillRepeatedly(Return(response));
        std::cout << "Mocked GET request to URI: " << uri << std::endl;
        std::cout << "Returning response: " << response.dump(4) << std::endl;
    }

    void MockInitializationSeq(std::string deviceId = "device123");
    XSPD::Detector* MockInitialization(std::string deviceId = "device123");

    string expectedApiUri = "http://localhost:8080/api";
    string expectedDeviceUri = this->expectedApiUri + "/v1/devices";

    json sampleApiResponse = {{"api version", "1"}, {"libxsp version", "1.2.3"}};
    json sampleDeviceList = {
        {"devices", json::array({{{"id", "device123"}}, {{"id", "device456"}}})}};
    json sampleDeviceInfo = {
        {"system",
         {{"detectors", json::array({{{"id", "lambda"}, {"n_modules", 2}}})},
          {"data-ports",
           json::array({{{"id", "port01"}, {"ip", "192.168.1.1"}, {"port", 1234}},
                        {{"id", "port02"}, {"ip", "192.168.1.1"}, {"port", 5678}}})}}}};
    json sampleModuleInfo = {
        {"module", "module0"}, {"firmware", "v1.0"}, {"chip_ids", json::array({"chipA", "chipB"})}};

    json sampleInfoVar = {
        {"libxsp version", "1.2.3"},
        {"detectors",
         json::array({{{"detector-id", "lambda"}, {"modules", this->sampleModuleInfo}}})}};

    std::map<std::string, std::string> sampleVarResponse = {{"status", "1"},
                                                            {"message", "success"}};
};

#endif  // ADXSPD_TESTS_XSPD_SERVICE_H
