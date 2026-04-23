
#include "XSPDAPI.h"

/**
 * @brief Parses a version string into its major, minor, and patch components
 *
 * @param versionStr The semantic version string to parse (e.g., "1.2.3")
 * @return A tuple containing the major, minor, and patch version numbers
 */
tuple<int, int, int> XSPD::ParseVersionString(const string& versionStr) {
    stringstream versionStream(versionStr);
    string segment;
    vector<int> parts;

    while (getline(versionStream, segment, '.')) {
        try {
            parts.push_back(stoi(segment));
        } catch (invalid_argument& e) {
            parts.push_back(0);
        }
    }

    if (parts.size() >= 3) {
        return {parts[0], parts[1], parts[2]};
    } else if (parts.size() == 2) {
        return {parts[0], parts[1], 0};
    } else if (parts.size() == 1) {
        return {parts[0], 0, 0};
    } else {
        return {0, 0, 0};
    }
}

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

    auto [xspdMajorVer, xspdMinorVer, xspdPatchVer] = ParseVersionString(this->xspdVersion);
    if (xspdMajorVer < MIN_XSPD_MAJOR_VERSION ||
        (xspdMajorVer == MIN_XSPD_MAJOR_VERSION && xspdMinorVer < MIN_XSPD_MINOR_VERSION) ||
        (xspdMajorVer == MIN_XSPD_MAJOR_VERSION && xspdMinorVer == MIN_XSPD_MINOR_VERSION &&
         xspdPatchVer < MIN_XSPD_PATCH_VERSION)) {
        throw runtime_error("XSPD version " + this->xspdVersion +
                            " is not supported. Minimum required version is " +
                            to_string(MIN_XSPD_MAJOR_VERSION) + "." +
                            to_string(MIN_XSPD_MINOR_VERSION) + "." +
                            to_string(MIN_XSPD_PATCH_VERSION) + ".");
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
    json deviceInfo;
    try {
        deviceInfo = GetVar<json>("info");
        this->systemId = deviceInfo["id"].get<string>();
    } catch (out_of_range& e) {
        throw runtime_error("Failed to retrieve device info for device ID " + this->deviceId +
                            ": " + string(e.what()));
    }

    // Get libxsp version if available
    if (deviceInfo.contains("libxsp version"))
        this->libxspVersion = deviceInfo["libxsp version"].get<string>();

    if (!deviceInfo.contains("detectors") || deviceInfo["detectors"].empty())
        throw runtime_error("No detector information found for device ID " + this->deviceId);

    // Only support single detector for now
    json detectorInfo = deviceInfo["detectors"][0];
    if (!detectorInfo.contains("detector-id") || !detectorInfo.contains("modules"))
        throw runtime_error(
            "Detector information is missing 'detector-id' or 'modules' field for device ID " +
            this->deviceId);

    this->detector = new Detector(this, detectorInfo["detector-id"].get<string>());
    for (auto& moduleJson : detectorInfo["modules"]) {
        int numChips = moduleJson["chips"].get<int>();
        vector<string> chipIds;
        for (int i = 0; i < numChips; i++) {
            chipIds.push_back(moduleJson["chip-ids"][i].get<string>());
        }
        Module* pmodule = new Module(this, moduleJson["module"].get<string>(),
                                     moduleJson["firmware"].get<string>(), chipIds);
        this->detector->RegisterModule(pmodule);
    }

    json dataPortInfo = Get("devices/" + this->deviceId)["system"]["data-ports"];
    if (dataPortInfo.empty())
        throw runtime_error("No data ports found for device ID " + this->deviceId);

    for (auto& dpInfo : dataPortInfo) {
        if (!dpInfo.contains("id") || !dpInfo.contains("ip") || !dpInfo.contains("port"))
            throw runtime_error(
                "Data port information is missing 'id', 'ip', or 'port' field for device ID " +
                this->deviceId);

        DataPort* pdataPort = new DataPort(this, dpInfo["id"].get<string>(),
                                           dpInfo["ip"].get<string>(), dpInfo["port"].get<int>());
        this->detector->RegisterDataPort(pdataPort);
    }

    return this->detector;
}

/**
 * @brief Retrieves the libxsp version
 *
 * @return string The libxsp version string
 */
string XSPD::API::GetLibXSPVersion() {
    if (this->libxspVersion.empty()) throw runtime_error("XSPD API not initialized!");
    return this->libxspVersion;
}

/**
 * @brief Retrieves the API version
 *
 * @return string The API version string
 */
string XSPD::API::GetApiVersion() {
    if (this->apiVersion.empty()) throw runtime_error("XSPD API not initialized!");
    return this->apiVersion;
}

/**
 * @brief Retrieves the XSPD version
 *
 * @return string The XSPD version string
 */
string XSPD::API::GetXSPDVersion() {
    if (this->xspdVersion.empty()) throw runtime_error("XSPD API not initialized!");
    return this->xspdVersion;
}

/**
 * @brief Retrieves the device ID
 *
 * @return string The device ID string
 */
string XSPD::API::GetDeviceId() {
    if (this->deviceId.empty()) throw runtime_error("XSPD API not initialized!");
    return this->deviceId;
}

/**
 * @brief Retrieves the system ID
 *
 * @return string The system ID string
 */
string XSPD::API::GetSystemId() {
    if (this->systemId.empty()) throw runtime_error("XSPD API not initialized!");
    return this->systemId;
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
        throw invalid_argument("Must set low threshold before setting high threshold");

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
    vector<double> rbThresholds = this->SetVar<string, vector<double>>("thresholds", thresholdsStr);

    if (rbThresholds.size() <= static_cast<size_t>(threshold))
        throw runtime_error("Failed to set " + thresholdName +
                            " threshold, readback size is less than expected");

    return rbThresholds[static_cast<size_t>(threshold)];
}

vector<XSPD::ModuleFeature> XSPD::Module::GetFeatures() {
    vector<string> featureStrings = this->GetVar<vector<string>>("features");
    vector<XSPD::ModuleFeature> features;
    for (auto& featureStr : featureStrings) {
        auto feature = magic_enum::enum_cast<XSPD::ModuleFeature>(featureStr);
        // TODO: Handle error case
        if (feature.has_value()) {
            features.push_back(feature.value());
        }
    }
    return features;
}

/**
 * @brief gets the serial number of the detector.
 *
 * Since libxsp does not currently provide a way of reading the serial number directly, it must be
 * stored in user data by the user. This function attempts to read the serial number from user data,
 * and if it is not found there, it falls back to returning the system ID, which is unique to each
 * device and can serve as an identifier in place of the serial number.
 *
 * @return string The serial number of the detector, or the system ID if the s/n is not available in
 * user data
 */
string XSPD::Detector::GetSerialNumber() {
    string sn = this->GetUserDataVar<string>("serial_number");
    // Fall back to system ID if serial number is not available in user data
    if (sn.empty()) sn = this->GetAPI()->GetSystemId();
    return sn;
}
