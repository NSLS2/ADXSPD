
#include "XSPDAPI.h"

/**
 * @brief Checks if a device with the given device ID exists
 *
 * @param deviceId The device ID to check
 * @return true if the device exists, false otherwise
 */
bool XSPD::API::DeviceExists(string deviceId) {
    json devices;
    try {
        devices = this->Get("devices")["devices"];
    } catch (json::out_of_range& e) {
        throw runtime_error("No devices found: " + string(e.what()));
    }

    for (auto& device : devices) {
        if (device["id"] == deviceId) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Retrieves the device ID at the specified index
 *
 * @param deviceIndex The index of the device
 * @return The device ID as a string
 */
string XSPD::API::GetDeviceAtIndex(int deviceIndex) {
    json devices;
    try {
        devices = this->Get("devices")["devices"];
    } catch (json::out_of_range& e) {
        throw runtime_error("No devices found: " + string(e.what()));
    }

    if (deviceIndex < 0 || deviceIndex >= static_cast<int>(devices.size()))
        throw out_of_range("Device index " + to_string(deviceIndex) + " is out of range.");

    return devices[deviceIndex]["id"].get<string>();
}

/**
 * @brief Initializes the API and connects to the specified device
 *
 * @param deviceId The device ID to connect to (if empty, connects to the first device)
 * @return Pointer to the initialized Detector object
 */
XSPD::Detector* XSPD::API::Initialize(string deviceId) {
    // Get API version information
    json apiVersionInfo = SubmitRequest(this->baseUri + "/api", XSPD::RequestType::GET);

    // Extract version strings for api and xspd
    try {
        this->apiVersion = apiVersionInfo["api version"].get<string>();
        this->xspdVersion = apiVersionInfo["xspd version"].get<string>();
    } catch (json::type_error& e) {
        throw runtime_error("Failed to retrieve API version information: " + string(e.what()));
    }

    // Identify the deviceId to connect to
    if (deviceId.length() == 1 && isdigit(deviceId[0])) {
        int deviceIndex = stoi(deviceId);
        this->deviceId = GetDeviceAtIndex(deviceIndex);
    } else if (!deviceId.empty()) {
        if (!DeviceExists(deviceId)) {
            throw invalid_argument("Device with ID " + deviceId + " does not exist.");
        }
        this->deviceId = deviceId;
    } else {
        this->deviceId = GetDeviceAtIndex(0);  // Default to first device
    }

    // Retrieve detector information
    json detectorInfo = GetVar<json>("info");
    if (!detectorInfo.contains("detectors") || detectorInfo["detectors"].empty())
        throw runtime_error("No detector information found for device ID " + this->deviceId);

    // Get libxsp version if available
    if (detectorInfo.contains("libxsp version"))
        this->libxspVersion = detectorInfo["libxsp version"].get<string>();

    // Only support single detector for now
    detectorInfo = detectorInfo["detectors"][0];
    if (!detectorInfo.contains("detector-id") || !detectorInfo.contains("modules"))
        throw runtime_error(
            "Detector information is missing 'detector-id' or 'modules' field for device ID " +
            this->deviceId);

    this->detector = new Detector(this, detectorInfo["detector-id"].get<string>());
    for (auto& moduleJson : detectorInfo["modules"]) {
        vector<string> chipIds;
        for (auto& chipId : moduleJson["chip-ids"]) {
            chipIds.push_back(chipId.get<string>());
        }
        Module* pmodule = new Module(this, moduleJson["module"].get<string>(),
                                     moduleJson["firmware"].get<string>(), chipIds);
        this->detector->RegisterModule(pmodule);
    }

    try {
        json dataPortInfo = Get("devices/" + this->deviceId)["system"]["data-ports"];
        for (auto& dpInfo : dataPortInfo) {
            DataPort* pdataPort =
                new DataPort(this, dpInfo["id"].get<string>(), dpInfo["ip"].get<string>(),
                             dpInfo["port"].get<int>());
            this->detector->RegisterDataPort(pdataPort);
        }

    } catch (json::other_error& e) {
        throw runtime_error("Failed to retrieve data port information for " + this->deviceId +
                            ": " + string(e.what()));
    }

    return this->detector;
}

/**
 * @brief Retrieves the libxsp version
 * 
 * @return string The libxsp version string
 */
string XSPD::API::GetLibXSPVersion() {
    if (this->libxspVersion.empty()) throw runtime_error("XSPD API not initialized");
    return this->libxspVersion;
}

/**
 * @brief Retrieves the API version
 * 
 * @return string The API version string
 */
string XSPD::API::GetApiVersion() {
    if (this->apiVersion.empty()) throw runtime_error("XSPD API not initialized");
    return this->apiVersion;
}

/**
 * @brief Retrieves the XSPD version
 * 
 * @return string The XSPD version string
 */
string XSPD::API::GetXSPDVersion() {
    if (this->xspdVersion.empty()) throw runtime_error("XSPD API not initialized");
    return this->xspdVersion;
}

/**
 * @brief Submits an HTTP request to the XSPD API and returns the parsed JSON response
 *
 * @param uri The full URI to make the request to
 * @param reqType The type of HTTP request (GET, PUT, etc.)
 * @return json Parsed JSON response from the API
 */
json XSPD::API::SubmitRequest(string uri, XSPD::RequestType reqType) {
    cpr::Response response;
    string verbMsg;
    switch (reqType) {
        case XSPD::RequestType::GET:
            response = cpr::Get(cpr::Url(uri));
            verbMsg = "get data from " + uri;
            break;
        case XSPD::RequestType::PUT:
            response = cpr::Put(cpr::Url(uri));
            verbMsg = "put data to " + uri;
            break;
        default:
            throw invalid_argument("Unsupported request type");
    }

    if (response.status_code != 200)
        throw runtime_error("Failed to " + verbMsg + ": " + response.error.message);

    try {
        json parsedResponse = json::parse(response.text, nullptr, true, false, true);
        if (parsedResponse.empty()) throw runtime_error("Empty JSON response from " + uri);

        std::cout << parsedResponse.dump(4) << std::endl;
        return parsedResponse;
    } catch (json::parse_error& e) {
        throw runtime_error("Failed to parse JSON response from " + uri + ": " + string(e.what()));
    }
}

/**
 * @brief Makes a GET request to the XSPD API and returns the parsed JSON response
 *
 * @param uri The full URI to make the GET request to
 * @return json Parsed JSON response from the API
 */
json XSPD::API::Get(string endpoint) {
    // Make a GET request to the XSPD API

    string fullUri = this->baseUri + "/api/v" + this->GetApiVersion() + "/" + endpoint;

    return SubmitRequest(fullUri, XSPD::RequestType::GET);
}

/**
 * @brief Makes a GET request to the XSPD API and returns the parsed JSON response
 *
 * @param uri The full URI to make the GET request to
 * @return json Parsed JSON response from the API
 */
json XSPD::API::Put(string endpoint) {
    // Make a PUT request to the XSPD API

    string fullUri = this->baseUri + "/api/v" + this->GetApiVersion() + "/" + endpoint;

    return SubmitRequest(fullUri, XSPD::RequestType::PUT);
}

/**
 * @brief Executes a command on the connected device. First checks if the command string is valid.
 *
 * @param command The command to execute
 */
void XSPD::API::ExecCommand(string command) {
    json availableCommands = Get("devices/" + this->deviceId + "/commands");
    bool commandFound = false;
    for (auto& cmd : availableCommands) {
        if (cmd["path"] == command) {
            commandFound = true;
            break;
        }
    }

    if (!commandFound)
        throw invalid_argument("Command '" + command + "' not found for device ID " +
                               this->deviceId);

    Put("devices/" + this->deviceId + "/commands?path=" + command);
}

/**
 * @brief Sets the threshold value for the detector
 *
 * @param threshold The threshold type (LOW or HIGH)
 * @param value The threshold value to set
 * @return The readback threshold value after setting
 */
double XSPD::Detector::SetThreshold(XSPD::Threshold threshold, double value) {
    string thresholdName = (threshold == XSPD::Threshold::LOW) ? "Low" : "High";

    vector<double> thresholds = this->GetVar<vector<double>>("thresholds");
    if (thresholds.size() == 0 && threshold != XSPD::Threshold::LOW)
        throw runtime_error("Must set low threshold before setting high threshold");

    switch (thresholds.size()) {
        case 2:
            thresholds[static_cast<int>(threshold)] = value;
            break;
        case 1:
            if (threshold == XSPD::Threshold::LOW) {
                thresholds[0] = value;
                break;
            }
        default:
            thresholds.push_back(value);
            break;
    }

    string thresholdsStr = "";
    for (auto& threshold : thresholds) {
        thresholdsStr += to_string(threshold);
        if (&threshold != &thresholds.back()) {
            thresholdsStr += ",";
        }
    }

    // Thresholds set as comma-separated string, read as vector<double>
    string rbThresholdsStr = this->SetVar<string>("thresholds", thresholdsStr, "thresholds");
    vector<double> rbThresholds = json::parse(rbThresholdsStr.c_str()).get<vector<double>>();

    if (rbThresholds.size() <= static_cast<size_t>(threshold))
        throw runtime_error("Failed to set " + thresholdName +
                            " threshold, readback size is less than expected");

    return rbThresholds[static_cast<size_t>(threshold)];
}
