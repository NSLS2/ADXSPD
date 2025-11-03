#include "xspdSimulator.h"

XspdSimulator::XspdSimulator(int ctrlPort, int dataPort) : ctrlPort(ctrlPort), dataPort(dataPort), app() {    
    CROW_ROUTE(this->app, "/api")
    ([this]{
        crow::json::wvalue apiInfo({{"api version", "1"}, {"xspd version", "1.5.0"}});
        return apiInfo;
    });

    CROW_ROUTE(this->app, "/api/v1/devices")
    ([this](){
        crow::json::wvalue devices = crow::json::wvalue({
            {"devices", crow::json::wvalue::list({
                crow::json::wvalue({{"id", this->deviceId}}),
                crow::json::wvalue({{"type", "xspd"}}),
                crow::json::wvalue({{"href", "http://localhost:" + to_string(this->ctrlPort) + "/api/v1/devices/" + this->deviceId}})
            })}
        });
        return devices;
    });

    CROW_ROUTE(this->app, "/api/v1/devices/simdet")
    ([this](){
        crow::json::wvalue deviceInfo;
        deviceInfo["id"] = this->deviceId;
        deviceInfo["type"] = "xsp";
        deviceInfo["configfile"] = "/opt/xsp/config/system.yml";
        deviceInfo["system"] = crow::json::wvalue({
            {"id", "SYS"},
            {"detectors", crow::json::wvalue::list({
                crow::json::wvalue({
                    {"id", this->detectorId},
                    {"modules", 1}
                })
            })}
        });
        deviceInfo["dataPorts"] = crow::json::wvalue::list({
            crow::json::wvalue({
                {"ref", this->detectorId + "/1"},
                {"id", this->dataPortId},
                {"ip", "localhost"},
                {"port", this->dataPort},
            })
        });

        return deviceInfo;
    });

    CROW_ROUTE(this->app, "/api/v1/devices/simdet/variables?path=<string>")
    ([this](string varName){
        crow::json::wvalue resp;
        if (this->variables.find(varName) != this->variables.end()){
            resp["path"] = varName;
            resp["value"] = this->variables[varName];
        } else {
            resp["error"] = "unknown variable";
        }
        return resp;
    });

    this->start();
}

XspdSimulator::~XspdSimulator(){
    this->stop();
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
    cout << "Shutting down REST API server..." << endl;
    this->app.stop();
    if (this->restServerThread.joinable()){
        cout << "Joining REST API server thread..." << endl;
        this->restServerThread.join();
    }

    cout << "Stopping ZMQ Publisher..." << endl;
    zmq_close(this->zmqPublisher);
    zmq_ctx_destroy(this->zmqContext);
    if (this->zmqPubThread.joinable()){
        cout << "Joining ZMQ Publisher thread..." << endl;
        this->zmqPubThread.join();
    }
    cout << "XspdSimulator stopped." << endl;
}


