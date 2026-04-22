#include "MockXSPDAPI.h"

#include <fstream>

using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::Throw;

/**
 * @brief Construct a new Mock XSPDAPI object
 */
MockXSPDAPI::MockXSPDAPI() : XSPD::API("localhost", 8008) {
    // Load sample responses from JSON file. The sample responses were pulled from
    // the simulator provided by X-Spectrum
    string sampleResponsesPath = "scripts/samples/single_module_xsp_sim.json";
    std::ifstream file(sampleResponsesPath);
    if (!file.is_open())
        throw std::runtime_error("Failed to open sample responses JSON file at " +
                                 sampleResponsesPath);

    this->sampleResponses = json::parse(file);
    // Add a second device
    this->sampleResponses["api/v1/devices"]["devices"].push_back({{"id", "device456"}});
};

/**
 * @brief Mocks the API version check during initialization
 *
 * @param alternateResponse Optional alternate response to return
 */
void MockXSPDAPI::MockAPIVersionCheck(json* alternateResponse) {
    string uri = "localhost:8008/api";
    json response = alternateResponse ? *alternateResponse : this->sampleResponses["api"];
    EXPECT_CALL(*this, SubmitRequest(uri, XSPD::RequestType::GET)).WillOnce(Return(response));
    std::cout << "Mocked GET request to endpoint: api" << std::endl;
    std::cout << "Returning response: " << response.dump(4) << std::endl;
}

/**
 * @brief Mocks a Get request to the API
 *
 * @param endpoint The endpoint to get
 * @param alternateResponse Optional alternate response to return
 */
void MockXSPDAPI::MockGetRequest(string endpoint, json* alternateResponse) {
    string fullEndpoint = "api/v1/" + endpoint;
    string uri = "localhost:8008/" + fullEndpoint;
    std::cout << "Mocking GET request to URI: " << fullEndpoint << std::endl;
    if (!this->sampleResponses.contains(fullEndpoint)) {
        EXPECT_CALL(*this, SubmitRequest(uri, XSPD::RequestType::GET))
            .WillOnce(Throw(std::runtime_error("Failed to get data from " + uri)));
        std::cout << "Mocking non 200 response code." << std::endl;
    } else {
        json response =
            alternateResponse ? *alternateResponse : this->sampleResponses[fullEndpoint];
        EXPECT_CALL(*this, SubmitRequest(uri, XSPD::RequestType::GET)).WillOnce(Return(response));
        std::cout << "Returning response: " << response.dump(4) << std::endl;
    }
}

/**
 * @brief Mocks a GetVar request to the API
 *
 * @param variableEndpoint The variable endpoint to get
 * @param alternateResponse Optional alternate response to return
 */
void MockXSPDAPI::MockGetVarRequest(string variableEndpoint, json* alternateResponse) {
    this->MockGetRequest("devices/lambda01/variables?path=" + variableEndpoint, alternateResponse);
}

/**
 * @brief Mocks a repeated Get request to the API
 *
 * @param endpoint The endpoint to get
 * @param alternateResponse Optional alternate response to return
 */
void MockXSPDAPI::MockRepeatedGetRequest(string endpoint, json* alternateResponse) {
    string fullEndpoint = "api/v1/" + endpoint;
    string uri = "localhost:8008/" + fullEndpoint;
    json response = alternateResponse ? *alternateResponse : this->sampleResponses[fullEndpoint];
    EXPECT_CALL(*this, SubmitRequest(uri, XSPD::RequestType::GET)).WillRepeatedly(Return(response));
    std::cout << "Mocked GET request to URI: " << uri << std::endl;
    std::cout << "Returning response: " << response.dump(4) << std::endl;
}

/**
 * @brief Mocks a Set request to the API
 *
 * @param endpoint The endpoint to set
 * @param alternateResponse Optional alternate response to return
 */
void MockXSPDAPI::MockSetRequest(string endpoint, json* alternateResponse) {
    string fullEndpointWithArg = "api/v1/" + endpoint;
    string fullEndpoint = "api/v1/" + endpoint.substr(0, endpoint.find_last_of("&"));
    string uri = "localhost:8008/" + fullEndpointWithArg;
    string rbUri = "localhost:8008/" + fullEndpoint;

    string newValue = endpoint.substr(endpoint.find_last_of('=') + 1);

    // TODO: Thresholds are a vector but passed as a string value. Consider if there is a better way
    // of handling these
    if (rbUri.find("thresholds") != string::npos) {
        newValue = "[" + newValue + "]";
    }

    json newValueJson = json::parse(newValue);

    std::cout << "Mocked PUT request with value " << newValue << "to endpoint: " << fullEndpoint
              << std::endl;
    if (!this->sampleResponses.contains(fullEndpoint)) {
        EXPECT_CALL(*this, SubmitRequest(uri, XSPD::RequestType::PUT))
            .WillOnce(Throw(std::runtime_error("Failed to put data to " + uri)));
        std::cout << "Mocking non 200 response code." << std::endl;
    } else {
        // Update our sample responses json with the new value
        this->sampleResponses[fullEndpoint]["value"] = newValueJson;

        json response =
            alternateResponse ? *alternateResponse : this->sampleResponses[fullEndpoint];
        EXPECT_CALL(*this, SubmitRequest(uri, XSPD::RequestType::PUT)).WillOnce(Return(response));
        std::cout << "Returning response: " << response.dump(4) << std::endl;
    }
}

/**
 * @brief Mocks a SetVar request to the API
 *
 * @param variableEndpoint The variable endpoint to set
 * @param alternateResponse Optional alternate response to return
 */
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
    this->MockAPIVersionCheck();
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
