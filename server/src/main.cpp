#include "Config.h"
#include "ServerApp.h"

#include <iostream>

int main(int argc, char* argv[]) {
    try {
        auto config = projection::server::parseServerConfig(argc, argv);
        projection::server::ServerApp app(config);
        return app.run();
    } catch (const std::exception& ex) {
        std::cerr << "Server failed to start: " << ex.what() << std::endl;
        return 1;
    }
}
