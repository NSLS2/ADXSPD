#include "MockXSPDAPI.h"

using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SetArgPointee;



void MockXSPDAPI::MockGetRequest(string uri, json response) {
    EXPECT_CALL(*this, SubmitRequest(uri, XSPD::RequestType::GET))
        .WillOnce(Return(response));
    std::cout << "Mocked GET request to URI: " << uri << std::endl;
    std::cout << "Returning response: " << response.dump(4) << std::endl;
}

void MockXSPDAPI::MockRepeatedGetRequest(string uri, json response) {
    EXPECT_CALL(*this, SubmitRequest(uri, XSPD::RequestType::GET))
        .WillRepeatedly(Return(response));
    std::cout << "Mocked GET request to URI: " << uri << std::endl;
    std::cout << "Returning response: " << response.dump(4) << std::endl;
}

/**
 * @brief Mocks the sequence of API calls made during initialization
 *
 * @param deviceId The device ID to initialize with
 */
void MockXSPDAPI::MockInitializationSeq(std::string deviceId) {
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
XSPD::Detector* MockXSPDAPI::MockInitialization(std::string deviceId) {
    this->MockInitializationSeq(deviceId);
    return this->Initialize(deviceId);
}