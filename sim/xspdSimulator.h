#ifndef XSPD_SIMULATOR_H
#define XSPD_SIMULATOR_H

#include "crow.h"
#include <map>
#include <string>
#include <zmq.h>
#include <thread>

using namespace std;



class XspdSimulator {
public:
    XspdSimulator(int ctrlPort, int dataPort);
    ~XspdSimulator();

protected:

    thread restServerThread;
    thread zmqPubThread;
    map<string, string> detVariables = {
        // Detector-level parameters
        {this->detectorId + "/beam_energy", "12.0"},
        {this->detectorId + "/bit_depth", "16"},
        {this->detectorId + "/charge_summing", "0"},
        {this->detectorId + "/compression_level", "1"},
        {this->detectorId + "/compressor", "bslz4"},
        {this->detectorId + "/counter_mode", "normal"},
        {this->detectorId + "/countrate_correction", "1"},
        {this->detectorId + "/data_format", "hdf5"},
        {this->detectorId + "/detector_distance", "100.0"},
        {this->detectorId + "/detector_mode", "timing"},
        {this->detectorId + "/exposure_time", "1.0"},
        {this->detectorId + "/flatfield_normalization", "1"},
        {this->detectorId + "/flatfield_offset", "0"},
        {this->detectorId + "/flatfield_thresholds", "4000"},
        {this->detectorId + "/flatfield_correction", "1"},
        {this->detectorId + "/gating_mode", "0"},
        {this->detectorId + "/n_frames", "1"},
        {this->detectorId + "/n_modules", "1"},
        {this->detectorId + "/roi_rows", "514"},
        {this->detectorId + "/roi_columns", "1030"},
        {this->detectorId + "/saturation_correction", "1"},
        {this->detectorId + "/saturation_flag", "0"},
        {this->detectorId + "/shuffle_mode", "0"},
        {this->detectorId + "/shutter_time", "0.003"},
        {this->detectorId + "/status", "ready"},
        {this->detectorId + "/summed_frames", "1"},
        {this->detectorId + "/thresholds", "4000"},
        {this->detectorId + "/trigger_mode", "ints"},
        {this->detectorId + "/type", "Pilatus"},
        {this->detectorId + "/user_data", ""},
        // Module-specific parameters
        {this->detectorId + "1/chip_ids", "1,2,3,4,5,6,7,8"},
        {this->detectorId + "1/compression_level", "1"},
        {this->detectorId + "1/compressor", "bslz4"},
        {this->detectorId + "1/features", "pixel_mask,flatfield"},
        {this->detectorId + "1/firmware_version", "1.0.0"},
        {this->detectorId + "1/flatfield", ""},
        {this->detectorId + "1/flatfield_author", "simulator"},
        {this->detectorId + "1/flatfield_enabled", "0"},
        {this->detectorId + "1/flatfield_error", "0"},
        {this->detectorId + "1/flatfield_timestamp", "0"},
        {this->detectorId + "1/flatfield_status", "ready"},
        {this->detectorId + "1/frame_height", "514"},
        {this->detectorId + "1/frame_depth", "1"},
        {this->detectorId + "1/frame_width", "1030"},
        {this->detectorId + "1/frames_queued", "0"},
        {this->detectorId + "1/humidity", "45.0"},
        {this->detectorId + "1/interpolation", "0"},
        {this->detectorId + "1/max_frames", "1000"},
        {this->detectorId + "1/n_subframes", "1"},
        {this->detectorId + "1/n_connectors", "1"},
        {this->detectorId + "1/pixel_mask", ""},
        {this->detectorId + "1/pixel_mask_enabled", "0"},
        {this->detectorId + "1/position", "0,0,0"},
        {this->detectorId + "1/ram_allocated", "1073741824"},
        {this->detectorId + "1/rotation", "0.0"},
        {this->detectorId + "1/saturation_threshold", "65535"},
        {this->detectorId + "1/sensor_current", "0.001"},
        {this->detectorId + "1/shuffle_mode", "0"},
        {this->detectorId + "1/status", "ready"},
        {this->detectorId + "1/temperature", "25.0"},
        {this->detectorId + "1/voltage", "5.0"},
        // Data port parameters
        {this->dataPortId + "/frame_height", "514"},
        {this->dataPortId + "/frame_depth", "1"},
        {this->dataPortId + "/frame_width", "1030"},
        {this->dataPortId + "/frames_queued", "0"},
        {this->dataPortId + "/pixel_mask", ""},
    };

    void start();
    void stop();

    string deviceId = "simdet";
    string detectorId = "xspdsim";
    string dataPortId = "port-1";

    void* zmqContext;
    void* zmqPublisher;

    crow::SimpleApp app;
    int ctrlPort;
    int dataPort;
};


#endif // XSPD_SIMULATOR_H