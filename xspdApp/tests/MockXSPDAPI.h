#ifndef MOCK_XSPD_API_H
#define MOCK_XSPD_API_H


#include <gmock/gmock.h>

#include "XSPDAPI.h"

class MockXSPDAPI : public XSPD::API {
    public:
    MockXSPDAPI() : XSPD::API("localhost", 8080) {};
    MOCK_METHOD(json, SubmitRequest, (string uri, XSPD::RequestType reqType), (override));
    MOCK_METHOD(string, GetApiVersion, (), (override));
};

#endif // MOCK_XSPD_API_H