#ifndef MOCK_XSPDAPI_H
#define MOCK_XSPDAPI_H

#include <gmock/gmock.h>

#include "XSPDAPI.h"

class MockXSPDAPI : public XSPD::API {
   public:
    MockXSPDAPI();
    MOCK_METHOD(json, SubmitRequest, (string uri, XSPD::RequestType reqType), (override));
    // MOCK_METHOD(string, GetApiVersion, (), (override));
    void MockGetRequest(string endpoint, json* alternateResponse = nullptr);
    void MockGetVarRequest(string variableEndpoint, json* alternateResponse = nullptr);
    void MockRepeatedGetRequest(string endpoint, json* alternateResponse = nullptr);
    void MockInitializationSeq(string deviceId = "lambda01");
    // void MockIncompleteInitializationSeq(XSPD::APIState stopAtState, std::string deviceId =
    // "lambda01");
    XSPD::Detector* MockInitialization(string deviceId = "lambda01");
    void MockAPIVerionCheck();

    void UpdateSampleResp(string endpoint, json response) {
        string uri = "localhost:8008/api/v1/" + endpoint;
        this->sampleResponses[uri] = response;
    }
    json GetSampleResp(string endpoint) {
        string uri = "localhost:8008/api/v1/" + endpoint;
        return this->sampleResponses[uri];
    }

   private:
    json sampleResponses;
};

#endif  // MOCK_XSPDAPI_H
