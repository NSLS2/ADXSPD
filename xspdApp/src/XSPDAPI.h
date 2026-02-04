#ifndef XSPDAPI_H
#define XSPDAPI_H

#include <cpr/cpr.h>

#include <iostream>
#include <magic_enum/magic_enum.hpp>
#include <map>
#include <string>

#include "nlohmann/json.hpp"

using json = nlohmann::json;
using std::get;
using std::getline;
using std::invalid_argument;
using std::is_enum;
using std::is_same;
using std::map;
using std::out_of_range;
using std::runtime_error;
using std::stoi;
using std::string;
using std::stringstream;
using std::to_string;
using std::tuple;
using std::vector;

#define MIN_XSPD_MAJOR_VERSION 1
#define MIN_XSPD_MINOR_VERSION 5
#define MIN_XSPD_PATCH_VERSION 2

namespace XSPD {

enum class RequestType { GET, PUT, POST, DELETE };

enum class OnOff {
    OFF = 0,
    ON = 1,
};

enum class Compressor {
    NONE = 0,
    ZLIB = 1,
    BLOSC = 2,
};

enum class ShuffleMode {
    NO_SHUFFLE = 0,
    AUTO_SHUFFLE = 1,
    SHUFFLE_BIT = 2,
    SHUFFLE_BYTE = 3,
};

enum class TriggerMode {
    SOFTWARE = 0,
    EXT_FRAMES = 1,
    EXT_SEQ = 2,
};

enum class CounterMode {
    SINGLE = 0,
    DUAL = 1,
};

enum class Status {
    CONNECTED = 0,
    READY = 1,
    BUSY = 2,
};

enum class Threshold {
    LOW = 0,
    HIGH = 1,
};

enum class ModuleFeature {
    FEAT_HV = 0,
    FEAT_1_6_BIT = 1,
    FEAT_MEDIPIX_DAC_IO = 2,
    FEAT_EXTENDED_GATING = 3,
};

enum class APIState {
    NOT_INITIALIZED = 0,
    CHECKING_API_VERSION = 1,
    RETRIEVING_DEVICE_LIST = 2,
    RETRIEVING_DEVICE_INFO = 3,
    INITIALIZED = 4,
};

tuple<int, int, int> ParseVersionString(const string& versionStr);

// Forward declarations
class Module;
class DataPort;
class Detector;

class API {
   public:
    API(string hostname, int portNum) : baseUri(hostname + ":" + std::to_string(portNum)) {}
    Detector* Initialize(string deviceId = "");
    virtual ~API() {}

    void GetVersionInfo();
    string GetXSPDVersion();
    string GetLibXSPVersion();
    string GetApiVersion();
    string GetDeviceId();
    string GetSystemId();

    bool DeviceExists(string deviceId);
    string GetDeviceAtIndex(int deviceIndex);
    virtual json SubmitRequest(string uri, RequestType reqType);

    json Get(string endpoint);
    json Put(string endpoint);

    /**
     * @brief Retrieves the value of a variable from the API
     *
     * @tparam T The expected type of the variable
     * @param varPath The path to the variable
     * @param key The key within the JSON response to extract the value from (default is "value")
     * @return T The value of the variable
     */
    template <typename T>
    T GetVar(string varPath, string key = "value") {
        json response = Get("devices/" + this->deviceId + "/variables?path=" + varPath);
        return ReadVarFromResp<T>(response, varPath, key);
    }

    /**
     * @brief Sets the value of a variable in the API and returns the readback value
     *
     * @tparam SetT The type of the value to set
     * @tparam GetT The type of the readback value
     * @param varPath The path to the variable
     * @param value The value to set
     * @param rbKey The key within the JSON response to extract the readback value from (default is
     * "value")
     * @return GetT The readback value of the variable after setting
     */
    template <typename SetT, typename GetT>
    GetT SetVar(string varPath, SetT value, string rbKey = "value") {
        string valueAsStr;
        if constexpr (std::is_same_v<SetT, string>) {
            valueAsStr = value;
        } else if constexpr (is_enum<SetT>::value) {
            auto enumString = magic_enum::enum_name(value);
            if (enumString.empty()) {
                throw runtime_error("Failed to convert enum value to string for variable " +
                                    varPath);
            }
            valueAsStr = string(enumString);
        } else {
            valueAsStr = to_string(value);
        }

        json response = this->Put("devices/" + this->deviceId + "/variables?path=" + varPath +
                                  "&value=" + valueAsStr);
        return ReadVarFromResp<GetT>(response, varPath, rbKey);
    }

    /**
     * @brief Sets the value of a variable in the API and returns the readback value
     *
     * @tparam T The type of the variable to set and readback
     * @param varPath The path to the variable
     * @param value The value to set
     * @param rbKey The key within the JSON response to extract the readback value from (default is
     * "value")
     * @return T The readback value of the variable after setting
     */
    template <typename T>
    T SetVar(string varPath, T value, string rbKey = "value") {
        return this->SetVar<T, T>(varPath, value, rbKey);
    }

    void ExecCommand(string command);

    /**
     * @brief Reads a variable of type T from the JSON response
     *
     * @tparam T The expected type of the variable
     * @param response The JSON response from the API
     * @param varName The name of the variable (for error messages)
     * @param key The key within the JSON response to extract the value from
     * @return T The value of the variable
     */
    template <typename T>
    T ReadVarFromResp(json response, string varName, string key) {
        if (response.contains(key)) {
            if constexpr (is_enum<T>::value) {
                string valAsStr = response[key].get<string>();
                auto enumValue = magic_enum::enum_cast<T>(valAsStr, magic_enum::case_insensitive);
                if (enumValue.has_value()) {
                    return enumValue.value();
                } else {
                    throw runtime_error("Failed to cast value " + valAsStr +
                                        " to enum for variable " + varName);
                }
            } else if constexpr (is_same<T, bool>::value) {
                // Temporarily handle booleans manually, since they are being passed as strings
                // instead of bools.
                return response[key].get<string>() == "true";
            } else {
                return response[key].get<T>();
            }
        }
        throw out_of_range("Key " + key + " not found in response for variable " + varName);
    }

   private:
    string baseUri, apiVersion, xspdVersion, libxspVersion, deviceId, systemId;
    Detector* detector = nullptr;
};

class APIComponent {
   public:
    APIComponent(API* api, string id) : api(api), id(id) {}
    virtual ~APIComponent() = default;
    string GetId() { return this->id; }
    API* GetAPI() { return this->api; }

    /**
     * @brief Retrieves the value of a variable from the DataPort
     *
     * @tparam T The expected type of the variable
     * @param varName The name of the variable
     * @param key The key within the JSON response to extract the value from (default is "value")
     * @return T The value of the variable
     */
    template <typename T>
    T GetVar(string varName, string key = "value") {
        return this->api->GetVar<T>(this->id + "/" + varName, key);
    }

    /**
     * @brief Sets the value of a variable in the DataPort and returns the readback value
     *
     * @tparam SetT The type of the value to set
     * @tparam GetT The type of the readback value
     * @param varName The name of the variable
     * @param value The value to set
     * @param rbKey The key within the JSON response to extract the readback value from (default is
     * "value")
     * @return GetT The readback value of the variable after setting
     */
    template <typename SetT, typename GetT>
    GetT SetVar(string varName, SetT value, string rbKey = "value") {
        return this->api->SetVar<SetT, GetT>(this->id + "/" + varName, value, rbKey);
    }

    /**
     * @brief Sets the value of a variable in the DataPort
     *
     * @tparam T The type of the variable to set
     * @param varName The name of the variable
     * @param value The value to set
     * @param rbKey The key within the JSON response to extract the readback value from (default is
     * "value")
     * @return T The readback value of the variable after setting
     */
    template <typename T>
    T SetVar(string varName, T value, string rbKey = "value") {
        return this->api->SetVar<T>(this->id + "/" + varName, value, rbKey);
    }

   private:
    API* api;
    string id;
};

class DataPort : public APIComponent {
   public:
    DataPort(API* api, string id, string ip, int port)
        : APIComponent(api, id), ip(ip), port(port) {}
    virtual ~DataPort() = default;
    string GetURI() { return "tcp://" + this->ip + ":" + std::to_string(this->port); }

   private:
    string ip;
    int port;
};

class Module : public APIComponent {
   public:
    Module(API* api, string id, string moduleFirmware, vector<string> chipIds)
        : APIComponent(api, id), moduleFirmware(moduleFirmware), chipIds(chipIds) {}
    virtual ~Module() = default;
    string GetFirmware() { return this->moduleFirmware; }
    vector<string> GetChipIds() { return this->chipIds; }

   private:
    string moduleFirmware;
    vector<string> chipIds;
};

class Detector : public APIComponent {
   public:
    Detector(API* api, string id) : APIComponent(api, id) {}
    virtual ~Detector() = default;

    double SetThreshold(XSPD::Threshold threshold, double value);

    /**
     * @brief Updates and retrieves the current status of the detector
     *
     * @return Status The current status of the detector
     */
    Status UpdateStatus() {
        this->status = this->GetVar<Status>("status");
        return this->status;
    }

    Status GetStatus() { return this->status; }

    vector<Module*> GetModules() { return this->modules; }

    /**
     * @brief Retrieves the IDs of all registered data ports
     *
     * @return vector<string> A vector of data port IDs
     */
    vector<string> GetDataPortIds() {
        vector<string> dpIds;
        for (const auto& dpPair : this->dataPorts) {
            dpIds.push_back(dpPair.first);
        }
        return dpIds;
    }

    void RegisterModule(Module* module) { this->modules.push_back(module); }

    /**
     * @brief Registers a DataPort with the detector
     *
     * @param dataPort Pointer to the DataPort to register
     */
    void RegisterDataPort(DataPort* dataPort) {
        this->dataPorts[dataPort->GetId()] = dataPort;
        if (this->activeDataPort == nullptr) this->activeDataPort = dataPort;
    }

    /**
     * @brief Retrieves the firmware version(s) of the detector modules
     *
     * @return string The firmware version if all modules are identical, otherwise "Multiple
     * versions"
     */
    string GetFirmwareVersion() {
        if (this->modules.size() == 1) {
            return this->modules[0]->GetFirmware();
        } else {
            bool identicalFW = true;
            string fwVersion = this->modules[0]->GetFirmware();
            for (auto module : this->modules) {
                if (module->GetFirmware() != fwVersion) {
                    identicalFW = false;
                    break;
                }
            }
            if (identicalFW)
                return fwVersion;
            else
                return "Multiple versions";
        }
    };

    DataPort* GetActiveDataPort() { return this->activeDataPort; }

    void ExecCommand(string command) { this->GetAPI()->ExecCommand(this->GetId() + "/" + command); }

   private:
    Status status;
    vector<Module*> modules;
    map<string, DataPort*> dataPorts;
    DataPort* activeDataPort = nullptr;
};
};      // namespace XSPD
#endif  // XSPDAPI_H
