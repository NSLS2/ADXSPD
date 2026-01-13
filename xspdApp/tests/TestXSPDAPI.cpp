#include "TestXSPDAPI.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

/**
 * @brief Mocks the sequence of API calls made during initialization
 *
 * @param deviceId The device ID to initialize with
 */
void TestXSPDAPI::MockInitializationSeq(std::string deviceId) {
    InSequence seq;
    this->MockGetRequest(this->expectedApiUri, this->sampleApiResponse);
    this->MockGetRequest(this->expectedDeviceUri, this->sampleDeviceList);
    this->MockGetRequest(this->expectedDeviceUri + "/" + deviceId + "/variables?path=info",
                         this->sampleInfoVar);
    this->MockGetRequest(this->expectedDeviceUri + "/" + deviceId, this->sampleDeviceInfo);
}

/**
 * @brief Mocks the initialization process and returns the initialized Detector object
 *
 * @param deviceId The device ID to initialize with
 * @return XSPD::Detector* Pointer to the initialized Detector object
 */
XSPD::Detector* TestXSPDAPI::MockInitialization(std::string deviceId) {
    this->MockInitializationSeq(deviceId);
    return this->mockXSPDAPI->Initialize(deviceId);
}

TEST_F(TestXSPDAPI, TestGetApiVersionNotInitialized) {
    EXPECT_CALL(*mockXSPDAPI, GetApiVersion()).WillOnce(Invoke(mockXSPDAPI,
                                                              &MockXSPDAPI::API::GetApiVersion));
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

TEST_F(TestXSPDAPI, TestGetDeviceAtIndex) {
    this->MockRepeatedGetRequest(this->expectedDeviceUri, this->sampleDeviceList);
    std::string deviceId = mockXSPDAPI->GetDeviceAtIndex(0);
    ASSERT_EQ(deviceId, "device123");

    deviceId = mockXSPDAPI->GetDeviceAtIndex(1);
    ASSERT_EQ(deviceId, "device456");
}

TEST_F(TestXSPDAPI, TestDeviceExists) {
    this->MockRepeatedGetRequest(this->expectedDeviceUri, this->sampleDeviceList);
    ASSERT_EQ(mockXSPDAPI->DeviceExists("device123"), true);
    ASSERT_EQ(mockXSPDAPI->DeviceExists("device456"), true);
    ASSERT_EQ(mockXSPDAPI->DeviceExists("device789"), false);
}

TEST_F(TestXSPDAPI, TestAPIInitInvalidDeviceId) {
    this->MockInitializationSeq("device789");
    EXPECT_THAT([&]() { this->mockXSPDAPI->Initialize("device789"); },
                testing::ThrowsMessage<std::invalid_argument>(
                    testing::HasSubstr("Device with ID device789 does not exist.")));
}

TEST_F(TestXSPDAPI, TestAPIInitDeviceIndexOutOfRange) {
    this->MockInitializationSeq("5");
    EXPECT_THAT([&]() { this->mockXSPDAPI->Initialize("5"); },
                testing::ThrowsMessage<std::out_of_range>(
                    testing::HasSubstr("Device index 5 is out of range.")));
}

TEST_F(TestXSPDAPI, TestAPIInitNoDetectors) {
    InSequence seq;
    this->MockGetRequest(this->expectedApiUri, this->sampleApiResponse);
    this->MockGetRequest(this->expectedDeviceUri, this->sampleDeviceList);
    this->MockGetRequest(this->expectedDeviceUri + "/device123/variables?path=info", json());
    EXPECT_THAT([&]() { this->mockXSPDAPI->Initialize("device123"); },
                testing::ThrowsMessage<std::runtime_error>(
                    testing::HasSubstr("No detectors information found for device ID device123")));
}

TEST_F(TestXSPDAPI, TestAPIInitNoDataPorts) {
    InSequence seq;
    this->MockGetRequest(this->expectedApiUri, this->sampleApiResponse);
    this->MockGetRequest(this->expectedDeviceUri, this->sampleDeviceList);
    this->MockGetRequest(this->expectedDeviceUri + "device123/variables?path=info",
                         this->sampleInfoVar);
    this->MockGetRequest(this->expectedDeviceUri + "device123", json());
    EXPECT_THAT([&]() { this->mockXSPDAPI->Initialize("device123"); },
                testing::ThrowsMessage<std::runtime_error>(
                    testing::HasSubstr("No data-ports information found for device ID device123")));
}

TEST_F(TestXSPDAPI, TestAPIInitNoDeviceId) {
    this->MockInitializationSeq();
    XSPD::Detector* pdet = this->mockXSPDAPI->Initialize();
    ASSERT_EQ(this->mockXSPDAPI->GetDeviceId(), "device123");
    ASSERT_EQ(pdet->GetId(), "lambda");
    ASSERT_EQ(pdet->GetActiveDataPort()->GetId(), "port01");
}

TEST_F(TestXSPDAPI, TestAPIInitDeviceIndex) {
    this->MockInitializationSeq();
    XSPD::Detector* pdet = this->mockXSPDAPI->Initialize("0");
    ASSERT_EQ(this->mockXSPDAPI->GetDeviceId(), "device123");
    ASSERT_EQ(pdet->GetId(), "lambda");
    ASSERT_EQ(pdet->GetActiveDataPort()->GetId(), "port01");
}

TEST_F(TestXSPDAPI, TestAPIInitDeviceId) {
    this->MockInitializationSeq();
    XSPD::Detector* pdet = this->mockXSPDAPI->Initialize("device123");
    ASSERT_EQ(this->mockXSPDAPI->GetDeviceId(), "device123");
    ASSERT_EQ(pdet->GetId(), "lambda");
    ASSERT_EQ(pdet->GetActiveDataPort()->GetId(), "port01");
}

TEST_F(TestXSPDAPI, TestGetXSPDVersion) {
    this->MockGetRequest(this->expectedApiUri, sampleApiResponse);
    std::string xspdVersion = mockXSPDAPI->GetXSPDVersion();
    ASSERT_EQ(xspdVersion, "1.2.3");
}

TEST_F(TestXSPDAPI, TestGetValidEndpoint) {
    this->MockGetRequest("http://localhost:8080/api/v1/test-endpoint", json{{"key", "value"}});

    json response = mockXSPDAPI->Get("test-endpoint");
    ASSERT_EQ(response["key"], "value");
}

TEST_F(TestXSPDAPI, TestGetInvalidResponseCode) {
    EXPECT_CALL(*mockXSPDAPI, SubmitRequest("http://localhost:8080/api/v1/invalid-endpoint",
                                            XSPD::RequestType::GET))
        .WillOnce(testing::Throw(std::runtime_error("Failed to get data from invalid-endpoint")));

    ASSERT_THAT(
        [&]() { mockXSPDAPI->Get("invalid-endpoint"); },
        testing::ThrowsMessage<std::runtime_error>(testing::HasSubstr("Failed to get data from")));
}

TEST_F(TestXSPDAPI, TestReadVarFromRespValidInt) {
    json response = json{{"status", 1}};

    int status = mockXSPDAPI->ReadVarFromResp<int>(response, "status", "status");
    ASSERT_EQ(status, 1);
}

TEST_F(TestXSPDAPI, TestReadVarFromRespValidString) {
    json response = json{{"message", "success"}};

    std::string message = mockXSPDAPI->ReadVarFromResp<std::string>(response, "message", "message");
    ASSERT_EQ(message, "success");
}

TEST_F(TestXSPDAPI, TestReadVarFromRespKeyNotFound) {
    json response = json{{"status", 1}};

    EXPECT_THAT(
        [&]() { mockXSPDAPI->ReadVarFromResp<std::string>(response, "message", "message"); },
        testing::ThrowsMessage<std::out_of_range>(
            testing::HasSubstr("not found in response for variable")));
}

TEST_F(TestXSPDAPI, TestReadVarFromRespEnumValid) {
    json response = json{{"enumKey", "ON"}};

    XSPD::OnOff enumValue =
        mockXSPDAPI->ReadVarFromResp<XSPD::OnOff>(response, "enumVar", "enumKey");
    ASSERT_EQ(enumValue, XSPD::OnOff::ON);
}

TEST_F(TestXSPDAPI, TestReadVarFromRespEnumInvalid) {
    json response = json{{"enumKey", "HI"}};

    ASSERT_THAT(
        [&]() { mockXSPDAPI->ReadVarFromResp<XSPD::OnOff>(response, "enumVar", "enumKey"); },
        testing::ThrowsMessage<std::runtime_error>(testing::HasSubstr("Failed to cast value")));
}

TEST_F(TestXSPDAPI, TestReadVarFromRespVectorOfDoublesValid) {
    json response = json{{"values", json::array({1.1, 2.2, 3.3})}};

    std::vector<double> values =
        mockXSPDAPI->ReadVarFromResp<std::vector<double>>(response, "var", "values");
    ASSERT_EQ((int) values.size(), 3);
    ASSERT_DOUBLE_EQ(values[0], 1.1);
    ASSERT_DOUBLE_EQ(values[1], 2.2);
    ASSERT_DOUBLE_EQ(values[2], 3.3);
}

TEST_F(TestXSPDAPI, TestReadVarFromRespVectorOfIntsValid) {
    json response = json{{"values", json::array({1, 2, 3, 4})}};

    std::vector<int> values =
        mockXSPDAPI->ReadVarFromResp<std::vector<int>>(response, "var", "values");
    ASSERT_EQ((int) values.size(), 4);
    ASSERT_EQ(values[0], 1);
    ASSERT_EQ(values[1], 2);
    ASSERT_EQ(values[2], 3);
    ASSERT_EQ(values[3], 4);
}

TEST_F(TestXSPDAPI, TestGetIntDetectorVar) {
    json response = json{{"value", 42}};

    EXPECT_CALL(
        *mockXSPDAPI,
        SubmitRequest(
            "http://localhost:8080/api/v1/devices/device123/variables?path=lambda/testIntVar",
            XSPD::RequestType::GET))
        .WillOnce(Return(response));

    XSPD::Detector* pdet = this->MockInitialization("device123");
    int intValue = pdet->GetVar<int>("testIntVar");
    ASSERT_EQ(intValue, 42);
}


TEST_F(TestXSPDAPI, TestSettingHighThresholdFirstThrowsError) {
    XSPD::Detector* pdet = this->MockInitialization();

    EXPECT_CALL(*mockXSPDAPI,
                SubmitRequest(
                    "http://localhost:8080/api/v1/devices/device123/variables?path=lambda/thresholds",
                    XSPD::RequestType::GET))
        .WillOnce(Return(json{{"value", "[]"}}));

    EXPECT_THAT(
        [&]() { pdet->SetThreshold(XSPD::Threshold::HIGH, 100); },
        testing::ThrowsMessage<std::invalid_argument>(
            testing::HasSubstr("Must set low threshold before setting high threshold")));
}
