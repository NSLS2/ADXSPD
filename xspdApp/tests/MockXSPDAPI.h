#ifndef MOCK_XSPDAPI_H
#define MOCK_XSPDAPI_H

#include <gmock/gmock.h>

#include "XSPDAPI.h"

class MockXSPDAPI : public XSPD::API {
   public:
    MockXSPDAPI() : XSPD::API("localhost", 8080){};
    MOCK_METHOD(json, SubmitRequest, (string uri, XSPD::RequestType reqType), (override));
    // MOCK_METHOD(string, GetApiVersion, (), (override));
    void MockGetRequest(string uri, json response);
    void MockRepeatedGetRequest(string uri, json response);
    void MockInitializationSeq(string deviceId = "device123");
    XSPD::Detector* MockInitialization(string deviceId = "device123");

    string expectedApiUri = "localhost:8080/api";
    string expectedDeviceUri = this->expectedApiUri + "/v1/devices";
    string device123VarUri = this->expectedDeviceUri + "/device123/variables?path=";

    json sampleApiResponse = {{"api version", "1"}, {"xspd version", "1.2.3"}};
    json sampleDeviceList = {
        {"devices", json::array({{{"id", "device123"}}, {{"id", "device456"}}})}};
    json sampleDeviceInfo = {
        {"system",
         {{"detectors", json::array({{{"id", "lambda"}, {"n_modules", 2}}})},
          {"data-ports",
           json::array({{{"id", "port01"}, {"ip", "192.168.1.1"}, {"port", 1234}},
                        {{"id", "port02"}, {"ip", "192.168.1.1"}, {"port", 5678}}})}}}};
    json sampleModuleInfo = {
        {"module", "module0"}, {"firmware", "v1.0"}, {"chip-ids", json::array({"chipA", "chipB"})}};

    json sampleInfoVar = {
        {"path", "info"},
        {"value", {
        {"libxsp version", "4.5.6"},
        {"detectors",
         json::array({{{"detector-id", "lambda"}, {"modules", json::array({this->sampleModuleInfo})}}})}}}};

    std::map<std::string, std::string> sampleVarResponse = {{"status", "1"},
                                                            {"message", "success"}};
};

#endif  // MOCK_XSPDAPI_H
