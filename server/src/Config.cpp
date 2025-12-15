#include "Config.h"

#include <stdexcept>
#include <string>

namespace projection::server {

namespace {

constexpr const char* kDefaultDbPath = "./data/db/projection.db";
constexpr int kDefaultPort = 8080;
constexpr const char* kDefaultRendererHost = "127.0.0.1";
constexpr int kDefaultRendererPort = 5050;
constexpr bool kDefaultVerbose = false;
constexpr int kDefaultRendererConnectRetries = 30;

bool startsWith(const std::string& value, const std::string& prefix) {
    return value.rfind(prefix, 0) == 0;
}

int parsePort(const std::string& value) {
    try {
        int port = std::stoi(value);
        if (port <= 0 || port > 65535) {
            throw std::invalid_argument("port out of range");
        }
        return port;
    } catch (const std::exception&) {
        throw std::invalid_argument("Invalid port value: " + value);
    }
}

std::string parseOptionValue(int& index, int argc, char* argv[], const std::string& option) {
    if (index + 1 >= argc) {
        throw std::invalid_argument("Missing value for " + option);
    }
    ++index;
    return argv[index];
}

}  // namespace

ServerConfig parseServerConfig(int argc, char* argv[]) {
    ServerConfig config{kDefaultDbPath, kDefaultPort, kDefaultRendererHost, kDefaultRendererPort, kDefaultVerbose,
                        kDefaultRendererConnectRetries};

    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--db") {
            config.databasePath = parseOptionValue(i, argc, argv, "--db");
        } else if (startsWith(arg, "--db=")) {
            config.databasePath = arg.substr(5);
            if (config.databasePath.empty()) {
                throw std::invalid_argument("Missing value for --db");
            }
        } else if (arg == "--port") {
            config.httpPort = parsePort(parseOptionValue(i, argc, argv, "--port"));
        } else if (startsWith(arg, "--port=")) {
            config.httpPort = parsePort(arg.substr(7));
        } else if (arg == "--renderer-host") {
            config.rendererHost = parseOptionValue(i, argc, argv, "--renderer-host");
        } else if (startsWith(arg, "--renderer-host=")) {
            config.rendererHost = arg.substr(16);
            if (config.rendererHost.empty()) {
                throw std::invalid_argument("Missing value for --renderer-host");
            }
        } else if (arg == "--renderer-port") {
            config.rendererPort = parsePort(parseOptionValue(i, argc, argv, "--renderer-port"));
        } else if (startsWith(arg, "--renderer-port=")) {
            config.rendererPort = parsePort(arg.substr(16));
        } else if (arg == "--renderer-connect-retries") {
            config.rendererConnectRetries = parsePort(parseOptionValue(i, argc, argv, "--renderer-connect-retries"));
        } else if (startsWith(arg, "--renderer-connect-retries=")) {
            config.rendererConnectRetries = parsePort(arg.substr(28));
        } else if (arg == "--verbose") {
            config.verbose = true;
        } else {
            throw std::invalid_argument("Unknown argument: " + arg);
        }
    }

    return config;
}

}  // namespace projection::server
