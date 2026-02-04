#include "MockXSPDAPI.h"

#include <fstream>

using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::Throw;

MockXSPDAPI::MockXSPDAPI() : XSPD::API("localhost", 8008) {
    string sampleResponsesPath = "xspdApp/tests/samples/xspd_sample_resp_sim.json";
    std::ifstream file(sampleResponsesPath);
    if (!file.is_open())
        throw std::runtime_error("Failed to open sample responses JSON file at " +
                                 sampleResponsesPath);

    this->sampleResponses = json::parse(file);
    // Add a second device
    this->sampleResponses["localhost:8008/api/v1/devices"]["devices"].push_back(
        {{"id", "device456"}});
};

void MockXSPDAPI::MockAPIVerionCheck(json* alternateResponse) {
    string uri = "localhost:8008/api";
    json response = alternateResponse ? *alternateResponse : this->sampleResponses[uri];
    EXPECT_CALL(*this, SubmitRequest(uri, XSPD::RequestType::GET)).WillOnce(Return(response));
    std::cout << "Mocked GET request to URI: " << uri << std::endl;
    std::cout << "Returning response: " << response.dump(4) << std::endl;
}

void MockXSPDAPI::MockGetRequest(string endpoint, json* alternateResponse) {
    string uri = "localhost:8008/api/v1/" + endpoint;
    if (!this->sampleResponses.contains(uri)) {
        EXPECT_CALL(*this, SubmitRequest(uri, XSPD::RequestType::GET))
            .WillOnce(Throw(std::runtime_error("Failed to get data from " + uri)));
        std::cout << "Mocked GET request to URI: " << uri << std::endl;
        std::cout << "Mocking non 200 response code." << std::endl;
    } else {
        json response = alternateResponse ? *alternateResponse : this->sampleResponses[uri];
        EXPECT_CALL(*this, SubmitRequest(uri, XSPD::RequestType::GET)).WillOnce(Return(response));
        std::cout << "Mocked GET request to URI: " << uri << std::endl;
        std::cout << "Returning response: " << response.dump(4) << std::endl;
    }
}

void MockXSPDAPI::MockGetVarRequest(string variableEndpoint, json* alternateResponse) {
    this->MockGetRequest("devices/lambda01/variables?path=" + variableEndpoint, alternateResponse);
}

void MockXSPDAPI::MockRepeatedGetRequest(string endpoint, json* alternateResponse) {
    string uri = "localhost:8008/api/v1/" + endpoint;
    json response = alternateResponse ? *alternateResponse : this->sampleResponses[uri];
    EXPECT_CALL(*this, SubmitRequest(uri, XSPD::RequestType::GET)).WillRepeatedly(Return(response));
    std::cout << "Mocked GET request to URI: " << uri << std::endl;
    std::cout << "Returning response: " << response.dump(4) << std::endl;
}

void MockXSPDAPI::MockSetRequest(string endpoint, json* alternateResponse) {
    string uri = "localhost:8008/api/v1/" + endpoint;
    string rbUri = "localhost:8008/api/v1/" + endpoint.substr(0, endpoint.find_last_of("&"));

    string newValue = endpoint.substr(endpoint.find_last_of('=') + 1);

    // TODO: Thresholds are a vector but passed as a string value. Consider if there is a better way
    // of handling these
    if (rbUri.find("thresholds") != string::npos) {
        newValue = "[" + newValue + "]";
    }

    json newValueJson = json::parse(newValue);

    if (!this->sampleResponses.contains(rbUri)) {
        EXPECT_CALL(*this, SubmitRequest(uri, XSPD::RequestType::PUT))
            .WillOnce(Throw(std::runtime_error("Failed to put data to " + uri)));
        std::cout << "Mocked PUT request to URI: " << uri << std::endl;
        std::cout << "Mocking non 200 response code." << std::endl;
    } else {
        // Update our sample responses json with the new value
        this->sampleResponses[rbUri]["value"] = newValueJson;

        json response = alternateResponse ? *alternateResponse : this->sampleResponses[rbUri];
        EXPECT_CALL(*this, SubmitRequest(uri, XSPD::RequestType::PUT)).WillOnce(Return(response));
        std::cout << "Mocked PUT request to URI: " << uri << std::endl;
        std::cout << "Returning response: " << response.dump(4) << std::endl;
    }
}

void MockXSPDAPI::MockSetVarRequest(string variableEndpoint, json* alternateResponse) {
    this->MockSetRequest("devices/lambda01/variables?path=" + variableEndpoint, alternateResponse);
}

// void MockXSPDAPI::MockIncompleteInitializationSeq(XSPD::APIState stopAtState, std::string
// deviceId) {
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
//             throw std::invalid_argument("Invalid stopAtState for
//             MockIncompleteInitializationSeq");
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
