#include "ServerApp.h"

#include <iostream>

int main() {
    projection::server::ServerConfig config{
        .databasePath = "./data/db/projection.db",
        .httpPort = 8080,
    };

    try {
        projection::server::ServerApp app(config);
        return app.run();
    } catch (const std::exception& ex) {
        std::cerr << "Server failed to start: " << ex.what() << std::endl;
        return 1;
    }
}
