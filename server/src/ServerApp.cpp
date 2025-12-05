#include "ServerApp.h"

#include <chrono>
#include <iostream>
#include <thread>

namespace projection::server {

ServerApp::ServerApp(ServerConfig config) : config_(std::move(config)) {}

int ServerApp::run() {
    std::cout << "Starting projection server on port " << config_.httpPort
              << " using database at '" << config_.databasePath << "'" << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    std::cout << "Server initialization complete" << std::endl;

    return 0;
}

}  // namespace projection::server
