#include "xspdSimulator.h"
#include <iostream>
#include <csignal>

volatile sig_atomic_t stopRequested = 0;

void handleSignal(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        stopRequested = 1;
    }
}

int main(int argc, char* argv[]) {
    signal(SIGINT, handleSignal);
    signal(SIGTERM, handleSignal);

    XspdSimulator simulator(8000, 8080);
    while (!stopRequested) {
        this_thread::sleep_for(chrono::milliseconds(100));
    }
    return 0;
}