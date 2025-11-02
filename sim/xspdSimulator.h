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
    
    void start();
    void stop();
    map<string, string> variables;
    crow::json::wvalue constructVariableResp(string varName);

    string deviceId = "xspdSimulator";
    string detectorId = "sim-det-1";
    string dataPortId = "port-1";

    void* zmqContext;
    void* zmqPublisher;

    crow::SimpleApp app;
    int ctrlPort;
    int dataPort;
};


#endif // XSPD_SIMULATOR_H