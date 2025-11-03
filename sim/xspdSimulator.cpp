#include "xspdSimulator.h"

XspdSimulator::XspdSimulator(int ctrlPort, int dataPort) : ctrlPort(ctrlPort), dataPort(dataPort), app() {    
    CROW_ROUTE(this->app, "/api")
    ([this]{
        crow::json::wvalue apiInfo({{"api version", "1"}, {"xspd version", "1.5.0"}});
        return apiInfo;
    });

    CROW_ROUTE(this->app, "/api/v1/devices")
    ([this](){
        crow::json::wvalue devices;
        vector<crow::json::wvalue> deviceList = crow::json::wvalue::list();
        crow::json::wvalue device;
        device["id"] = this->detectorId;
        device["dataPortId"] = this->dataPortId;
        deviceList.push_back(device);
        devices["devices"] = deviceList;
        return devices;
    });

    CROW_ROUTE(this->app, "/api/v1/devices/xspdSimulator/variables?path=<string>")
    ([this](string varName){
        return this->constructVariableResp(varName);
    });

    this->start();
}

XspdSimulator::~XspdSimulator(){
    this->stop();
}

crow::json::wvalue XspdSimulator::constructVariableResp(string varName){
    crow::json::wvalue resp;
    if (this->variables.find(varName) != this->variables.end()){
        resp["path"] = varName;
        resp["value"] = this->variables[varName];
    } else {
        resp["error"] = "unknown variable";
    }
    return resp;
} 


void XspdSimulator::start(){
    this->restServerThread = thread([this] {
        this->app.port(this->ctrlPort).multithreaded().run_async();
    });
    cout << "XspdSimulator REST API server started on port " << this->ctrlPort << endl;

    this->zmqContext = zmq_ctx_new();
    this->zmqPublisher = zmq_socket(zmqContext, ZMQ_PUB);
    string bindAddress = "tcp://*:" + to_string(dataPort);
    this->zmqPubThread = thread([this, bindAddress]{
        zmq_bind(this->zmqPublisher, bindAddress.c_str());
    });
    cout << "XspdSimulator ZMQ Publisher started on port " << this->dataPort << endl;
}

void XspdSimulator::stop(){
    this->app.stop();
    if (this->restServerThread.joinable()){
        this->restServerThread.join();
    }
    zmq_close(this->zmqPublisher);
    zmq_ctx_destroy(this->zmqContext);
    if (this->zmqPubThread.joinable()){
        this->zmqPubThread.join();
    }
    cout << "XspdSimulator stopped." << endl;
}


