#include "xspdSimulator.h"
#include <cpr/cpr.h>
#include <iostream>

int main(int argc, char* argv[]) {

    XspdSimulator simulator(8000, 8080);

    bool run = true;
    string command;
    while (run) {
        cout << "xspdSim > ";
        getline(cin, command);

        if (command == "exit") {
            run = false;
        } else if (command == "help") {
            cout << "Available commands:\n";
            cout << "  help - Show this help message\n";
            cout << "  exit - Exit the simulator\n";
            cout << "  <endpoint> - Send a GET request to the specified REST API endpoint\n";
        } else if (!command.empty()) {
            string url = "http://localhost:8000/" + command;
            cout << "Sending GET request to: " << url << endl;

            // Use a simple HTTP client to send the request
            cpr::Response res = cpr::Get(cpr::Url{url});

            cout << "Response Code: " << res.status_code << endl;
            cout << "Response Body: " << res.text << endl;
        }
    }

    return 0;
}