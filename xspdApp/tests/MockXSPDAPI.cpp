#include "MockXSPDAPI.h"
#include <fstream>

using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Throw;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SetArgPointee;

MockXSPDAPI::MockXSPDAPI() : XSPD::API("localhost", 8008) {
    std::ifstream file("xspdApp/tests/samples/xspd_sample_resp_sim.json");
    if (!file.is_open())
        throw std::runtime_error("Failed to open sample responses JSON file.");

    this->sampleResponses = json::parse(file);
    // Add a second device
    this->sampleResponses["localhost:8008/api/v1/devices"]["devices"].push_back({{"id", "device456"}});
};

void MockXSPDAPI::MockAPIVerionCheck() {
    string uri = "localhost:8008/api";
    json response = this->sampleResponses[uri];
    EXPECT_CALL(*this, SubmitRequest(uri, XSPD::RequestType::GET))
        .WillOnce(Return(response));
    std::cout << "Mocked GET request to URI: " << uri << std::endl;
    std::cout << "Returning response: " << response.dump(4) << std::endl;
}

void MockXSPDAPI::MockGetRequest(string endpoint, json* alternateResponse) {
    string uri = "localhost:8008/api/v1/" + endpoint;
    if (!this->sampleResponses.contains(uri)){
        EXPECT_CALL(*this, SubmitRequest(uri, XSPD::RequestType::GET))
            .WillOnce(Throw(std::runtime_error("Failed to get data from " + uri)));
        std::cout << "Mocked GET request to URI: " << uri << std::endl;
        std::cout << "Mocking non 200 response code." << std::endl;
    } else {
        json response = alternateResponse ? *alternateResponse : this->sampleResponses[uri];
        EXPECT_CALL(*this, SubmitRequest(uri, XSPD::RequestType::GET))
            .WillOnce(Return(response));
        std::cout << "Mocked GET request to URI: " << uri << std::endl;
        std::cout << "Returning response: " << response.dump(4) << std::endl;
    }
}

void MockXSPDAPI::MockGetVarRequest(string variableEndpoint, json* alternateResponse) {
    this->MockGetRequest("devices/lambda01/variables?path=" + variableEndpoint,
                          alternateResponse);
}

void MockXSPDAPI::MockRepeatedGetRequest(string endpoint, json* alternateResponse) {
    string uri = "localhost:8008/api/v1/" + endpoint;
    json response = alternateResponse ? *alternateResponse : this->sampleResponses[uri];
    EXPECT_CALL(*this, SubmitRequest(uri, XSPD::RequestType::GET))
        .WillRepeatedly(Return(response));
    std::cout << "Mocked GET request to URI: " << uri << std::endl;
    std::cout << "Returning response: " << response.dump(4) << std::endl;
}

// void MockXSPDAPI::MockIncompleteInitializationSeq(XSPD::APIState stopAtState, std::string deviceId) {
//     InSequence seq;
//     switch (stopAtState) {
//         case stopAtState < XSPD::APIState::CHECKING_API_VERSION:
//             this->MockAPIVerionCheck();
//             break;
//         case XSPD::APIState::RETRIEVING_DEVICE_LIST:
//             this->MockAPIVerionCheck();
//             this->MockGetRequest("devices");
//             break;
//         case XSPD::APIState::RETRIEVING_DEVICE_INFO:
//             this->MockAPIVerionCheck();
//             this->MockGetRequest("devices");
//             this->MockGetRequest("devices/" + deviceId + "/variables?path=info");
//             break;
//         default:
//             throw std::invalid_argument("Invalid stopAtState for MockIncompleteInitializationSeq");
//     }
// }

/**
 * @brief Mocks the sequence of API calls made during initialization
 *
 * @param deviceId The device ID to initialize with
 */
void MockXSPDAPI::MockInitializationSeq(std::string deviceId) {
    InSequence seq;
    this->MockAPIVerionCheck();
    this->MockGetRequest("devices");
    this->MockGetRequest("devices/" + deviceId + "/variables?path=info");
    this->MockGetRequest("devices/" + deviceId);
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