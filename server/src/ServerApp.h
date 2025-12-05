#pragma once

#include <string>

namespace projection::server {

struct ServerConfig {
    std::string databasePath;
    int httpPort;
};

class ServerApp {
public:
    explicit ServerApp(ServerConfig config);

    int run();

private:
    ServerConfig config_;
};

}  // namespace projection::server
