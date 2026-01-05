#include <nlohmann/json.hpp>

#include "ADXSPDTest.h"

TEST_F(ADXSPDTest, TestXSPDGet) {
    json resp = xspdDriver->xspdGet("localhost:8000/api");
    EXPECT_FALSE(resp.is_null());
    EXPECT_TRUE(resp.contains("api version"));
    EXPECT_TRUE(resp.contains("xspd version"));
    EXPECT_EQ(resp["api version"].get<std::string>(), "1");
    EXPECT_EQ(resp["xspd version"].get<std::string>(), "simulator-0.1");
}

TEST_F(ADXSPDTest, TestXSPDGetInvalidEndpoint) {
    json resp = xspdDriver->xspdGet("localhost:8000/api/invalid_endpoint");
    EXPECT_TRUE(resp.is_null());
}
