#include "TestXSPDAPI.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>


TEST_F(TestXSPDAPI, TestGetApiVersionNotInitialized) {
    EXPECT_THAT([&]() { this->mockXSPDAPI->GetApiVersion(); },
                testing::ThrowsMessage<std::runtime_error>(
                    testing::HasSubstr("XSPD API not initialized")));
}

TEST_F(TestXSPDAPI, TestGetXSPDVersionNotInitialized) {
    EXPECT_THAT([&]() { this->mockXSPDAPI->GetXSPDVersion(); },
                testing::ThrowsMessage<std::runtime_error>(
                    testing::HasSubstr("XSPD API not initialized")));
}

TEST_F(TestXSPDAPI, TestGetLibXSPVersionNotInitialized) {
    EXPECT_THAT([&]() { this->mockXSPDAPI->GetLibXSPVersion(); },
                testing::ThrowsMessage<std::runtime_error>(
                    testing::HasSubstr("XSPD API not initialized")));
}

TEST_F(TestXSPDAPI, TestGetVersionInfoAfterInitialization) {
    this->mockXSPDAPI->MockInitialization();
    ASSERT_EQ(this->mockXSPDAPI->GetApiVersion(), "1");
    ASSERT_EQ(this->mockXSPDAPI->GetXSPDVersion(), "1.5.2");
    ASSERT_EQ(this->mockXSPDAPI->GetLibXSPVersion(), "2.7.5");
}

TEST_F(TestXSPDAPI, TestGetDeviceAtIndex) {
    this->mockXSPDAPI->MockInitialization();
    this->mockXSPDAPI->MockRepeatedGetRequest("devices");
    std::string deviceId = mockXSPDAPI->GetDeviceAtIndex(0);
    ASSERT_EQ(deviceId, "lambda01");

    deviceId = mockXSPDAPI->GetDeviceAtIndex(1);
    ASSERT_EQ(deviceId, "device456");
}

TEST_F(TestXSPDAPI, TestDeviceExists) {
    this->mockXSPDAPI->MockInitialization();
    this->mockXSPDAPI->MockRepeatedGetRequest("devices");
    ASSERT_EQ(mockXSPDAPI->DeviceExists("lambda01"), true);
    ASSERT_EQ(mockXSPDAPI->DeviceExists("device456"), true);
    ASSERT_EQ(mockXSPDAPI->DeviceExists("device789"), false);
}

TEST_F(TestXSPDAPI, TestAPIInitInvalidDeviceId) {
    InSequence seq;
    this->mockXSPDAPI->MockAPIVerionCheck();
    this->mockXSPDAPI->MockGetRequest("devices");
    EXPECT_THAT([&]() { this->mockXSPDAPI->Initialize("device789"); },
                testing::ThrowsMessage<std::invalid_argument>(
                    testing::HasSubstr("Device with ID device789 does not exist.")));
}

TEST_F(TestXSPDAPI, TestAPIInitDeviceIndexOutOfRange) {
    InSequence seq;
    this->mockXSPDAPI->MockAPIVerionCheck();
    this->mockXSPDAPI->MockGetRequest("devices");
    EXPECT_THAT([&]() { this->mockXSPDAPI->Initialize("5"); },
                testing::ThrowsMessage<std::out_of_range>(
                    testing::HasSubstr("Device index 5 is out of range.")));
}

TEST_F(TestXSPDAPI, TestAPIInitNoDeviceInfo) {
    InSequence seq;
    json emptyResponse = json();
    this->mockXSPDAPI->MockAPIVerionCheck();
    this->mockXSPDAPI->MockGetRequest("devices");
    this->mockXSPDAPI->MockGetVarRequest("info", &emptyResponse);
    EXPECT_THAT([&]() { this->mockXSPDAPI->Initialize("lambda01"); },
                testing::ThrowsMessage<std::runtime_error>(
                    testing::HasSubstr("Failed to retrieve device info for device ID lambda01")));
}

TEST_F(TestXSPDAPI, TestAPIInitNoDetectors) {
    InSequence seq;
    json modifiedInfoResp = this->mockXSPDAPI->GetSampleResp("devices/lambda01/variables?path=info");
    modifiedInfoResp["value"]["detectors"] = json::array();
    this->mockXSPDAPI->MockAPIVerionCheck();
    this->mockXSPDAPI->MockGetRequest("devices");
    this->mockXSPDAPI->MockGetVarRequest("info", &modifiedInfoResp);
    EXPECT_THAT([&]() { this->mockXSPDAPI->Initialize("lambda01"); },
                testing::ThrowsMessage<std::runtime_error>(
                    testing::HasSubstr("No detector information found for device ID lambda01")));
}

TEST_F(TestXSPDAPI, TestAPIInitNoDetectorID){
    InSequence seq;
    json modifiedInfoResp = this->mockXSPDAPI->GetSampleResp("devices/lambda01/variables?path=info");
    modifiedInfoResp["value"]["detectors"][0].erase("detector-id");
    this->mockXSPDAPI->MockAPIVerionCheck();
    this->mockXSPDAPI->MockGetRequest("devices");
    this->mockXSPDAPI->MockGetVarRequest("info", &modifiedInfoResp);
    EXPECT_THAT([&]() { this->mockXSPDAPI->Initialize("lambda01"); },
                testing::ThrowsMessage<std::runtime_error>(
                    testing::HasSubstr("Detector information is missing 'detector-id' or 'modules' field for device ID lambda01")));
}

TEST_F(TestXSPDAPI, TestAPIInitNoModules) {
    InSequence seq;
    json modifiedInfoResp = this->mockXSPDAPI->GetSampleResp("devices/lambda01/variables?path=info");
    modifiedInfoResp["value"]["detectors"][0].erase("modules");
    this->mockXSPDAPI->MockAPIVerionCheck();
    this->mockXSPDAPI->MockGetRequest("devices");
    this->mockXSPDAPI->MockGetVarRequest("info", &modifiedInfoResp);
    EXPECT_THAT([&]() { this->mockXSPDAPI->Initialize("lambda01"); },
                testing::ThrowsMessage<std::runtime_error>(
                    testing::HasSubstr("Detector information is missing 'detector-id' or 'modules' field for device ID lambda01")));
}

TEST_F(TestXSPDAPI, TestAPIInitNoDataPorts) {
    InSequence seq;
    json modifiedDeviceInfo = this->mockXSPDAPI->GetSampleResp("devices/lambda01");
    modifiedDeviceInfo["system"]["data-ports"] = json::array();
    this->mockXSPDAPI->MockAPIVerionCheck();
    this->mockXSPDAPI->MockGetRequest("devices");
    this->mockXSPDAPI->MockGetVarRequest("info");
    this->mockXSPDAPI->MockGetRequest("devices/lambda01", &modifiedDeviceInfo);
    EXPECT_THAT([&]() { this->mockXSPDAPI->Initialize("lambda01"); },
                testing::ThrowsMessage<std::runtime_error>(
                    testing::HasSubstr("No data ports found for device ID lambda01")));
}

TEST_F(TestXSPDAPI, TestAPIInitMissingDataPortInfo) {
    InSequence seq;
    json modifiedDeviceInfo = this->mockXSPDAPI->GetSampleResp("devices/lambda01");
    modifiedDeviceInfo["system"]["data-ports"][0].erase("ip");
    this->mockXSPDAPI->MockAPIVerionCheck();
    this->mockXSPDAPI->MockGetRequest("devices");
    this->mockXSPDAPI->MockGetVarRequest("info");
    this->mockXSPDAPI->MockGetRequest("devices/lambda01", &modifiedDeviceInfo);
    EXPECT_THAT([&]() { this->mockXSPDAPI->Initialize("lambda01"); },
                testing::ThrowsMessage<std::runtime_error>(
                    testing::HasSubstr("Data port information is missing 'id', 'ip', or 'port' field for device ID lambda01")));
}

TEST_F(TestXSPDAPI, TestAPIInitNoDeviceId) {
    this->mockXSPDAPI->MockInitializationSeq();
    XSPD::Detector* pdet = this->mockXSPDAPI->Initialize();
    ASSERT_EQ(this->mockXSPDAPI->GetDeviceId(), "lambda01");
    ASSERT_EQ(pdet->GetId(), "lambda");
    ASSERT_EQ(pdet->GetActiveDataPort()->GetId(), "port01");
}

TEST_F(TestXSPDAPI, TestAPIInitDeviceIndex) {
    this->mockXSPDAPI->MockInitializationSeq();
    XSPD::Detector* pdet = this->mockXSPDAPI->Initialize("0");
    ASSERT_EQ(this->mockXSPDAPI->GetDeviceId(), "lambda01");
    ASSERT_EQ(pdet->GetId(), "lambda");
    ASSERT_EQ(pdet->GetActiveDataPort()->GetId(), "port01");
}

TEST_F(TestXSPDAPI, TestAPIInitDeviceId) {
    this->mockXSPDAPI->MockInitializationSeq();
    XSPD::Detector* pdet = this->mockXSPDAPI->Initialize("lambda01");
    ASSERT_EQ(this->mockXSPDAPI->GetDeviceId(), "lambda01");
    ASSERT_EQ(pdet->GetId(), "lambda");
    ASSERT_EQ(pdet->GetActiveDataPort()->GetId(), "port01");
}

TEST_F(TestXSPDAPI, TestGetValidEndpoint) {
    this->mockXSPDAPI->MockInitialization();
    this->mockXSPDAPI->MockGetVarRequest("lambda/summed_frames");
    json response = this->mockXSPDAPI->Get("devices/lambda01/variables?path=lambda/summed_frames");
    ASSERT_EQ(response["path"], "lambda/summed_frames");
    ASSERT_EQ(response["value"], "1");
}

TEST_F(TestXSPDAPI, TestGetInvalidEndpoint) {
    this->mockXSPDAPI->MockInitialization();
    this->mockXSPDAPI->MockGetRequest("invalid/endpoint");
    EXPECT_THAT(
        [&]() {
            this->mockXSPDAPI->Get("invalid/endpoint");
        },
        testing::ThrowsMessage<std::runtime_error>(
            testing::HasSubstr("Failed to get data from localhost:8008/api/v1/invalid/endpoint")));
}

TEST_F(TestXSPDAPI, TestReadVarFromRespValidInt) {

    json response = json{{"status", 1}};
    int status = this->mockXSPDAPI->ReadVarFromResp<int>(response, "status", "status");
    ASSERT_EQ(status, 1);
}

TEST_F(TestXSPDAPI, TestReadVarFromRespValidString) {

    json response = json{{"message", "success"}};

    std::string message = this->mockXSPDAPI->ReadVarFromResp<std::string>(response, "message", "message");
    ASSERT_EQ(message, "success");
}

TEST_F(TestXSPDAPI, TestReadVarFromRespKeyNotFound) {
    json response = json{{"status", 1}};

    EXPECT_THAT(
        [&]() { this->mockXSPDAPI->ReadVarFromResp<std::string>(response, "message", "message"); },
        testing::ThrowsMessage<std::out_of_range>(
            testing::HasSubstr("not found in response for variable")));
}

TEST_F(TestXSPDAPI, TestReadVarFromRespEnumValid) {
    json response = json{{"enumKey", "ON"}};

    XSPD::OnOff enumValue =
        this->mockXSPDAPI->ReadVarFromResp<XSPD::OnOff>(response, "enumVar", "enumKey");
    ASSERT_EQ(enumValue, XSPD::OnOff::ON);
}

TEST_F(TestXSPDAPI, TestReadVarFromRespEnumInvalid) {
    json response = json{{"enumKey", "HI"}};

    ASSERT_THAT(
        [&]() { this->mockXSPDAPI->ReadVarFromResp<XSPD::OnOff>(response, "enumVar", "enumKey"); },
        testing::ThrowsMessage<std::runtime_error>(testing::HasSubstr("Failed to cast value")));
}

TEST_F(TestXSPDAPI, TestReadVarFromRespVectorOfDoublesValid) {
    json response = json{{"values", json::array({1.1, 2.2, 3.3})}};

    std::vector<double> values =
        this->mockXSPDAPI->ReadVarFromResp<std::vector<double>>(response, "var", "values");
    ASSERT_EQ((int) values.size(), 3);
    ASSERT_DOUBLE_EQ(values[0], 1.1);
    ASSERT_DOUBLE_EQ(values[1], 2.2);
    ASSERT_DOUBLE_EQ(values[2], 3.3);
}

TEST_F(TestXSPDAPI, TestReadVarFromRespVectorOfIntsValid) {
    json response = json{{"values", json::array({1, 2, 3, 4})}};

    std::vector<int> values =
        this->mockXSPDAPI->ReadVarFromResp<std::vector<int>>(response, "var", "values");
    ASSERT_EQ((int) values.size(), 4);
    ASSERT_EQ(values[0], 1);
    ASSERT_EQ(values[1], 2);
    ASSERT_EQ(values[2], 3);
    ASSERT_EQ(values[3], 4);
}

TEST_F(TestXSPDAPI, TestGetIntDetectorVar) {
    XSPD::Detector* pdet = this->mockXSPDAPI->MockInitialization();
    json response = json{{"value", 42}};

    this->mockXSPDAPI->MockGetVarRequest("lambda/bit_depth");

    int intValue = pdet->GetVar<int>("bit_depth");
    ASSERT_EQ(intValue, 12);
}


TEST_F(TestXSPDAPI, TestSettingHighThresholdFirstThrowsError) {
    XSPD::Detector* pdet = this->mockXSPDAPI->MockInitialization();

    this->mockXSPDAPI->MockGetVarRequest("lambda/thresholds");

    ASSERT_THAT(
        [&]() { pdet->SetThreshold(XSPD::Threshold::HIGH, 100); },
        testing::ThrowsMessage<std::invalid_argument>(
            testing::HasSubstr("Must set low threshold before setting high threshold")));
}
