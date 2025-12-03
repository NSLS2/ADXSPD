
#include "XSPDAPI.h"





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



XSPD::Detector* XSPD::API::Initialize(string deviceId) {

    json apiVersionInfo = SubmitRequest(this->baseUri + "/api", XSPD::RequestType::GET);

    try {
        this->apiVersion = apiVersionInfo["api version"].get<string>();
        this->xspdVersion = apiVersionInfo["xspd version"].get<string>();
    } catch(json::type_error& e) {
        throw runtime_error("Failed to retrieve API version information: " + string(e.what()));
    }

    // Determine which device to use
    if (deviceId.length() == 1 && isdigit(deviceId[0])) {
        int deviceIndex = stoi(deviceId);
        this->deviceId = GetDeviceAtIndex(deviceIndex);
    } else if (!deviceId.empty()) {
        if (!DeviceExists(deviceId)) {
            throw invalid_argument("Device with ID " + deviceId + " does not exist.");
        }
        this->deviceId = deviceId;
    } else {
        this->deviceId = GetDeviceAtIndex(0); // Default to first device
    }

    json detectorInfo = GetVar<json>("info");
    if (!detectorInfo.contains("detectors") || detectorInfo["detectors"].empty())
        throw runtime_error("No detector information found for device ID " + this->deviceId);

    // Get libxsp version if available
    if (detectorInfo.contains("libxsp version"))
        this->libxspVersion = detectorInfo["libxsp version"].get<string>();

    // Only support single detector for now
    detectorInfo = detectorInfo["detectors"][0];
    if (!detectorInfo.contains("detector-id") || !detectorInfo.contains("modules"))
        throw runtime_error("Detector information is missing 'detector-id' or 'modules' field for device ID " + this->deviceId);

    this->detector = new Detector(this, detectorInfo["detector-id"].get<string>());
    for(auto & moduleJson : detectorInfo["modules"]) {
        vector<string> chipIds;
        for (auto& chipId : moduleJson["chip-ids"]) {
            chipIds.push_back(chipId.get<string>());
        }
        Module* module = new Module(this,
                                    moduleJson["module"].get<string>(),
                                    moduleJson["firmware"].get<string>(),
                                    chipIds);
        this->detector->RegisterModule(module);
    }

    try {
        json dataPortInfo = Get("devices/" + this->deviceId)["system"]["data-ports"];
        for(auto & dpInfo : dataPortInfo) {
        DataPort* dp = new DataPort(this,
                                    dpInfo["id"].get<string>(),
                                    dpInfo["ip"].get<string>(),
                                    dpInfo["port"].get<int>());
        this->detector->RegisterDataPort(dp);
    }

    } catch (json::other_error& e) {
        throw runtime_error("Failed to retrieve data port information for " + this->deviceId + ": " + string(e.what()));
    }

    return this->detector;
}

string XSPD::API::GetLibXSPVersion() {
    if (this->libxspVersion.empty())
        throw runtime_error("XSPD API not initialized");
    return this->libxspVersion;
}

string XSPD::API::GetApiVersion() {
    if (this->apiVersion.empty())
        throw runtime_error("XSPD API not initialized");
    return this->apiVersion;
}

string XSPD::API::GetXSPDVersion() {
    if (this->xspdVersion.empty())
        throw runtime_error("XSPD API not initialized");
    return this->xspdVersion;
}


json XSPD::API::SubmitRequest(string uri, XSPD::RequestType reqType) {
    cpr::Response response;
    string verbMsg;
    switch(reqType) {
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
        if (parsedResponse.empty())
            throw runtime_error("Empty JSON response from " + uri);

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
        throw invalid_argument("Command '" + command + "' not found for device ID " + this->deviceId);

    Put("devices/" + this->deviceId + "/commands?path=" + command);
}
