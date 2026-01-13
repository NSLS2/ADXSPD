#ifndef MOCK_XSPDAPI_H
#define MOCK_XSPDAPI_H

#include <gmock/gmock.h>

#include "XSPDAPI.h"

class MockXSPDAPI : public XSPD::API {
<<<<<<< Updated upstream
   public:
    MockXSPDAPI() : XSPD::API("localhost", 8080){};
=======
    public:
    MockXSPDAPI() : XSPD::API("http://localhost", 8080) {};
>>>>>>> Stashed changes
    MOCK_METHOD(json, SubmitRequest, (string uri, XSPD::RequestType reqType), (override));
    MOCK_METHOD(string, GetApiVersion, (), (override));
};

#endif  // MOCK_XSPDAPI_H
