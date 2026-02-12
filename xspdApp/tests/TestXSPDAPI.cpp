#include "TestXSPDAPI.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

TEST_F(TestXSPDAPI, TestParseVersionString) {
    auto [major, minor, patch] = XSPD::ParseVersionString("1.2.3");
    ASSERT_EQ(major, 1);
    ASSERT_EQ(minor, 2);
    ASSERT_EQ(patch, 3);

    auto [major2, minor2, patch2] = XSPD::ParseVersionString("4.5");
    ASSERT_EQ(major2, 4);
    ASSERT_EQ(minor2, 5);
    ASSERT_EQ(patch2, 0);

    auto [major3, minor3, patch3] = XSPD::ParseVersionString("6");
    ASSERT_EQ(major3, 6);
    ASSERT_EQ(minor3, 0);
    ASSERT_EQ(patch3, 0);

    auto [major4, minor4, patch4] = XSPD::ParseVersionString("");
    ASSERT_EQ(major4, 0);
    ASSERT_EQ(minor4, 0);
    ASSERT_EQ(patch4, 0);
}

TEST_F(TestXSPDAPI, TestParseVersionStringNotNumeric) {
    auto [major, minor, patch] = XSPD::ParseVersionString("a.b.c");
    ASSERT_EQ(major, 0);
    ASSERT_EQ(minor, 0);
    ASSERT_EQ(patch, 0);
}

TEST_F(TestXSPDAPI, TestGetApiVersionNotInitialized) {
    EXPECT_THAT(
        [&]() { this->mapi->GetApiVersion(); },
        testing::ThrowsMessage<std::runtime_error>(testing::HasSubstr("XSPD API not initialized")));
}

TEST_F(TestXSPDAPI, TestGetXSPDVersionNotInitialized) {
    EXPECT_THAT(
        [&]() { this->mapi->GetXSPDVersion(); },
        testing::ThrowsMessage<std::runtime_error>(testing::HasSubstr("XSPD API not initialized")));
}

TEST_F(TestXSPDAPI, TestGetLibXSPVersionNotInitialized) {
    EXPECT_THAT(
        [&]() { this->mapi->GetLibXSPVersion(); },
        testing::ThrowsMessage<std::runtime_error>(testing::HasSubstr("XSPD API not initialized")));
}

TEST_F(TestXSPDAPI, TestGetDeviceIdNotInitialized) {
    EXPECT_THAT(
        [&]() { this->mapi->GetDeviceId(); },
        testing::ThrowsMessage<std::runtime_error>(testing::HasSubstr("XSPD API not initialized")));
}

TEST_F(TestXSPDAPI, TestGetSystemIdNotInitialized) {
    EXPECT_THAT(
        [&]() { this->mapi->GetSystemId(); },
        testing::ThrowsMessage<std::runtime_error>(testing::HasSubstr("XSPD API not initialized")));
}

TEST_F(TestXSPDAPI, TestInitializationFailsXSPDVersionInvalid) {
    json versionResp = json{
        {"xspd version", "0.9.0"},
        {"api version", "1"},
    };

    this->mapi->MockAPIVersionCheck(&versionResp);
    ASSERT_THAT([&]() { this->mapi->Initialize(); },
                testing::ThrowsMessage<std::runtime_error>(testing::HasSubstr(
                    "XSPD version 0.9.0 is not supported. Minimum required version is")));
}

TEST_F(TestXSPDAPI, TestGetDeviceIdAfterInitialization) {
    this->mapi->MockInitialization();
    ASSERT_EQ(this->mapi->GetDeviceId(), "lambda01");
}

TEST_F(TestXSPDAPI, TestGetSystemIdAfterInitialization) {
    this->mapi->MockInitialization();
    ASSERT_EQ(this->mapi->GetSystemId(), "SYSTEM");
}

TEST_F(TestXSPDAPI, TestGetVersionInfoAfterInitialization) {
    this->mapi->MockInitialization();
    ASSERT_EQ(this->mapi->GetApiVersion(), "1");
    ASSERT_EQ(this->mapi->GetXSPDVersion(), "1.6.0");
    ASSERT_EQ(this->mapi->GetLibXSPVersion(), "2.7.6");
}

TEST_F(TestXSPDAPI, TestGetDeviceAtIndex) {
    this->mapi->MockInitialization();
    this->mapi->MockRepeatedGetRequest("devices");
    std::string deviceId = mapi->GetDeviceAtIndex(0);
    ASSERT_EQ(deviceId, "lambda01");

    deviceId = mapi->GetDeviceAtIndex(1);
    ASSERT_EQ(deviceId, "device456");
}

TEST_F(TestXSPDAPI, TestDeviceExists) {
    this->mapi->MockInitialization();
    this->mapi->MockRepeatedGetRequest("devices");
    ASSERT_EQ(mapi->DeviceExists("lambda01"), true);
    ASSERT_EQ(mapi->DeviceExists("device456"), true);
    ASSERT_EQ(mapi->DeviceExists("device789"), false);
}

TEST_F(TestXSPDAPI, TestAPIInitInvalidDeviceId) {
    InSequence seq;
    this->mapi->MockAPIVersionCheck();
    this->mapi->MockGetRequest("devices");
    EXPECT_THAT([&]() { this->mapi->Initialize("device789"); },
                testing::ThrowsMessage<std::invalid_argument>(
                    testing::HasSubstr("Device with ID device789 does not exist.")));
}

TEST_F(TestXSPDAPI, TestAPIInitDeviceIndexOutOfRange) {
    InSequence seq;
    this->mapi->MockAPIVersionCheck();
    this->mapi->MockGetRequest("devices");
    EXPECT_THAT([&]() { this->mapi->Initialize("5"); },
                testing::ThrowsMessage<std::out_of_range>(
                    testing::HasSubstr("Device index 5 is out of range.")));
}

TEST_F(TestXSPDAPI, TestAPIInitNoDeviceInfo) {
    InSequence seq;
    json emptyResponse = json();
    this->mapi->MockAPIVersionCheck();
    this->mapi->MockGetRequest("devices");
    this->mapi->MockGetVarRequest("info", &emptyResponse);
    EXPECT_THAT([&]() { this->mapi->Initialize("lambda01"); },
                testing::ThrowsMessage<std::runtime_error>(
                    testing::HasSubstr("Failed to retrieve device info for device ID lambda01")));
}

TEST_F(TestXSPDAPI, TestAPIInitNoDetectors) {
    InSequence seq;
    json modifiedInfoResp =
        this->mapi->GetSampleResp("devices/lambda01/variables?path=info");
    modifiedInfoResp["value"]["detectors"] = json::array();
    this->mapi->MockAPIVersionCheck();
    this->mapi->MockGetRequest("devices");
    this->mapi->MockGetVarRequest("info", &modifiedInfoResp);
    EXPECT_THAT([&]() { this->mapi->Initialize("lambda01"); },
                testing::ThrowsMessage<std::runtime_error>(
                    testing::HasSubstr("No detector information found for device ID lambda01")));
}

TEST_F(TestXSPDAPI, TestAPIInitNoDetectorID) {
    InSequence seq;
    json modifiedInfoResp =
        this->mapi->GetSampleResp("devices/lambda01/variables?path=info");
    modifiedInfoResp["value"]["detectors"][0].erase("detector-id");
    this->mapi->MockAPIVersionCheck();
    this->mapi->MockGetRequest("devices");
    this->mapi->MockGetVarRequest("info", &modifiedInfoResp);
    EXPECT_THAT([&]() { this->mapi->Initialize("lambda01"); },
                testing::ThrowsMessage<std::runtime_error>(
                    testing::HasSubstr("Detector information is missing 'detector-id' or 'modules' "
                                       "field for device ID lambda01")));
}

TEST_F(TestXSPDAPI, TestAPIInitNoModules) {
    InSequence seq;
    json modifiedInfoResp =
        this->mapi->GetSampleResp("devices/lambda01/variables?path=info");
    modifiedInfoResp["value"]["detectors"][0].erase("modules");
    this->mapi->MockAPIVersionCheck();
    this->mapi->MockGetRequest("devices");
    this->mapi->MockGetVarRequest("info", &modifiedInfoResp);
    EXPECT_THAT([&]() { this->mapi->Initialize("lambda01"); },
                testing::ThrowsMessage<std::runtime_error>(
                    testing::HasSubstr("Detector information is missing 'detector-id' or 'modules' "
                                       "field for device ID lambda01")));
}

TEST_F(TestXSPDAPI, TestAPIInitNoDataPorts) {
    InSequence seq;
    json modifiedDeviceInfo = this->mapi->GetSampleResp("devices/lambda01");
    modifiedDeviceInfo["system"]["data-ports"] = json::array();
    this->mapi->MockAPIVersionCheck();
    this->mapi->MockGetRequest("devices");
    this->mapi->MockGetVarRequest("info");
    this->mapi->MockGetRequest("devices/lambda01", &modifiedDeviceInfo);
    EXPECT_THAT([&]() { this->mapi->Initialize("lambda01"); },
                testing::ThrowsMessage<std::runtime_error>(
                    testing::HasSubstr("No data ports found for device ID lambda01")));
}

TEST_F(TestXSPDAPI, TestAPIInitMissingDataPortInfo) {
    InSequence seq;
    json modifiedDeviceInfo = this->mapi->GetSampleResp("devices/lambda01");
    modifiedDeviceInfo["system"]["data-ports"][0].erase("ip");
    this->mapi->MockAPIVersionCheck();
    this->mapi->MockGetRequest("devices");
    this->mapi->MockGetVarRequest("info");
    this->mapi->MockGetRequest("devices/lambda01", &modifiedDeviceInfo);
    EXPECT_THAT([&]() { this->mapi->Initialize("lambda01"); },
                testing::ThrowsMessage<std::runtime_error>(
                    testing::HasSubstr("Data port information is missing 'id', 'ip', or 'port' "
                                       "field for device ID lambda01")));
}

TEST_F(TestXSPDAPI, TestAPIInitNoDeviceId) {
    this->mapi->MockInitializationSeq();
    XSPD::Detector* pdet = this->mapi->Initialize();
    ASSERT_EQ(this->mapi->GetDeviceId(), "lambda01");
    ASSERT_EQ(pdet->GetId(), "lambda");
    ASSERT_EQ(pdet->GetActiveDataPort()->GetId(), "port-1");
}

TEST_F(TestXSPDAPI, TestAPIInitDeviceIndex) {
    this->mapi->MockInitializationSeq();
    XSPD::Detector* pdet = this->mapi->Initialize("0");
    ASSERT_EQ(this->mapi->GetDeviceId(), "lambda01");
    ASSERT_EQ(pdet->GetId(), "lambda");
    ASSERT_EQ(pdet->GetActiveDataPort()->GetId(), "port-1");
}

TEST_F(TestXSPDAPI, TestAPIInitDeviceId) {
    this->mapi->MockInitializationSeq();
    XSPD::Detector* pdet = this->mapi->Initialize("lambda01");
    ASSERT_EQ(this->mapi->GetDeviceId(), "lambda01");
    ASSERT_EQ(pdet->GetId(), "lambda");
    ASSERT_EQ(pdet->GetActiveDataPort()->GetId(), "port-1");
}

TEST_F(TestXSPDAPI, TestGetValidEndpoint) {
    this->mapi->MockInitialization();
    this->mapi->MockGetVarRequest("lambda/summed_frames");
    json response = this->mapi->Get("devices/lambda01/variables?path=lambda/summed_frames");
    ASSERT_EQ(response["path"], "lambda/summed_frames");
    ASSERT_EQ(response["value"], 1);
}

TEST_F(TestXSPDAPI, TestGetInvalidEndpoint) {
    this->mapi->MockInitialization();
    this->mapi->MockGetRequest("invalid/endpoint");
    EXPECT_THAT([&]() { this->mapi->Get("invalid/endpoint"); },
                testing::ThrowsMessage<std::runtime_error>(testing::HasSubstr(
                    "Failed to get data from localhost:8008/api/v1/invalid/endpoint")));
}

TEST_F(TestXSPDAPI, TestReadVarFromRespValidInt) {
    json response = json{{"status", 1}};
    int status = this->mapi->ReadVarFromResp<int>(response, "status", "status");
    ASSERT_EQ(status, 1);
}

TEST_F(TestXSPDAPI, TestReadVarFromRespValidString) {
    json response = json{{"message", "success"}};

    std::string message =
        this->mapi->ReadVarFromResp<std::string>(response, "message", "message");
    ASSERT_EQ(message, "success");
}

TEST_F(TestXSPDAPI, TestReadVarFromRespKeyNotFound) {
    json response = json{{"status", 1}};

    EXPECT_THAT(
        [&]() { this->mapi->ReadVarFromResp<std::string>(response, "message", "message"); },
        testing::ThrowsMessage<std::out_of_range>(
            testing::HasSubstr("not found in response for variable")));
}

TEST_F(TestXSPDAPI, TestReadVarFromRespEnumValid) {
    json response = json{{"path", "enumVar"}, {"enumKey", "ON"}};

    XSPD::OnOff enumValue =
        this->mapi->ReadVarFromResp<XSPD::OnOff>(response, "enumVar", "enumKey");
    ASSERT_EQ(enumValue, XSPD::OnOff::ON);
}

TEST_F(TestXSPDAPI, TestReadVarFromRespEnumInvalid) {
    json response = json{{"enumKey", "HI"}};

    ASSERT_THAT(
        [&]() { this->mapi->ReadVarFromResp<XSPD::OnOff>(response, "enumVar", "enumKey"); },
        testing::ThrowsMessage<std::runtime_error>(testing::HasSubstr("Failed to cast value")));
}

TEST_F(TestXSPDAPI, TestReadVarFromRespVectorOfDoublesValid) {
    json response = json{{"values", json::array({1.1, 2.2, 3.3})}};

    std::vector<double> values =
        this->mapi->ReadVarFromResp<std::vector<double>>(response, "var", "values");
    ASSERT_EQ((int) values.size(), 3);
    ASSERT_DOUBLE_EQ(values[0], 1.1);
    ASSERT_DOUBLE_EQ(values[1], 2.2);
    ASSERT_DOUBLE_EQ(values[2], 3.3);
}

TEST_F(TestXSPDAPI, TestReadVarFromRespVectorOfIntsValid) {
    json response = json{{"values", json::array({1, 2, 3, 4})}};

    std::vector<int> values =
        this->mapi->ReadVarFromResp<std::vector<int>>(response, "var", "values");
    ASSERT_EQ((int) values.size(), 4);
    ASSERT_EQ(values[0], 1);
    ASSERT_EQ(values[1], 2);
    ASSERT_EQ(values[2], 3);
    ASSERT_EQ(values[3], 4);
}

TEST_F(TestXSPDAPI, TestReadVarFromRespPathDoesNotMatch) {
    json response = json{{"path", "some/other/path"}, {"value", 10}};

    ASSERT_THAT([&]() { this->mapi->ReadVarFromResp<int>(response, "some/path", "value"); },
                testing::ThrowsMessage<std::runtime_error>(testing::HasSubstr(
                    "Variable path mismatch: expected some/path, got some/other/path")));
}

TEST_F(TestXSPDAPI, TestGetIntDetectorVar) {
    XSPD::Detector* pdet = this->mapi->MockInitialization();

    this->mapi->MockGetVarRequest("lambda/bit_depth");

    int intValue = pdet->GetVar<int>("bit_depth");
    ASSERT_EQ(intValue, 12);
}

TEST_F(TestXSPDAPI, TestGetBoolModuleVar) {
    XSPD::Detector* pdet = this->mapi->MockInitialization();
    this->mapi->MockGetVarRequest("lambda/1/flatfield_enabled");

    bool boolValue = pdet->GetModules()[0]->GetVar<bool>("flatfield_enabled");

    ASSERT_EQ(boolValue, false);

    // Update to true
    this->mapi->UpdateSampleResp(
        "devices/lambda01/variables?path=lambda/1/flatfield_enabled", json{{"value", true}});
    this->mapi->MockGetVarRequest("lambda/1/flatfield_enabled");

    boolValue = pdet->GetModules()[0]->GetVar<bool>("flatfield_enabled");
    ASSERT_EQ(boolValue, true);
}

TEST_F(TestXSPDAPI, TestSettingHighThresholdFirstThrowsError) {
    XSPD::Detector* pdet = this->mapi->MockInitialization();

    this->mapi->MockGetVarRequest("lambda/thresholds");

    ASSERT_THAT([&]() { pdet->SetThreshold(XSPD::Threshold::HIGH, 100); },
                testing::ThrowsMessage<std::invalid_argument>(
                    testing::HasSubstr("Must set low threshold before setting high threshold")));
}

TEST_F(TestXSPDAPI, TestSetThresholdsLowLevel) {
    XSPD::Detector* pdet = this->mapi->MockInitialization();
    this->mapi->MockSetVarRequest("lambda/thresholds&value=1.000000,2.000000");

    vector<double> thresholds =
        pdet->SetVar<std::string, std::vector<double>>("thresholds", "1.000000,2.000000");
    size_t expectedSize = 2;
    ASSERT_EQ(thresholds.size(), expectedSize);
    ASSERT_DOUBLE_EQ(thresholds[0], 1.0);
    ASSERT_DOUBLE_EQ(thresholds[1], 2.0);
}

TEST_F(TestXSPDAPI, TestSetThresholdsCorrectOrder) {
    XSPD::Detector* pdet = this->mapi->MockInitialization();

    this->mapi->MockGetVarRequest("lambda/thresholds");
    this->mapi->MockSetVarRequest("lambda/thresholds&value=1.000000");

    double lowThreshold = pdet->SetThreshold(XSPD::Threshold::LOW, 1.0);
    ASSERT_DOUBLE_EQ(lowThreshold, 1.0);

    this->mapi->MockGetVarRequest("lambda/thresholds");
    this->mapi->MockSetVarRequest("lambda/thresholds&value=1.000000,5.000000");

    double highThreshold = pdet->SetThreshold(XSPD::Threshold::HIGH, 5.0);
    ASSERT_DOUBLE_EQ(highThreshold, 5.0);

    this->mapi->MockGetVarRequest("lambda/thresholds");
    this->mapi->MockSetVarRequest("lambda/thresholds&value=1.000000,7.000000");

    highThreshold = pdet->SetThreshold(XSPD::Threshold::HIGH, 7.0);
    ASSERT_DOUBLE_EQ(highThreshold, 7.0);

    this->mapi->MockGetVarRequest("lambda/thresholds");
    this->mapi->MockSetVarRequest("lambda/thresholds&value=2.000000,7.000000");
    lowThreshold = pdet->SetThreshold(XSPD::Threshold::LOW, 2.0);
    ASSERT_DOUBLE_EQ(lowThreshold, 2.0);
}

TEST_F(TestXSPDAPI, TestGetModuleFeatures) {
    XSPD::Detector* pdet = this->mapi->MockInitialization();

    XSPD::Module* pmod = pdet->GetModules()[0];

    this->mapi->MockGetVarRequest("lambda/1/features");

    vector<XSPD::ModuleFeature> features = pmod->GetFeatures();
    size_t expectedSize = 3;
    ASSERT_EQ(features.size(), expectedSize);
    ASSERT_TRUE(std::find(features.begin(), features.end(), XSPD::ModuleFeature::FEAT_HV) != features.end());
    ASSERT_TRUE(std::find(features.begin(), features.end(), XSPD::ModuleFeature::FEAT_1_6_BIT) != features.end());
    ASSERT_TRUE(std::find(features.begin(), features.end(), XSPD::ModuleFeature::FEAT_EXTENDED_GATING) != features.end());
    ASSERT_TRUE(std::find(features.begin(), features.end(), XSPD::ModuleFeature::FEAT_MEDIPIX_DAC_IO) == features.end());
}