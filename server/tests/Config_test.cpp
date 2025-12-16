#include "Config.h"

#include <catch2/catch_test_macros.hpp>
#include <stdexcept>
#include <vector>

namespace projection::server {

namespace {
ServerConfig parseArgs(const std::vector<const char*>& args) {
    return parseServerConfig(static_cast<int>(args.size()), const_cast<char**>(args.data()));
}

bool throwsInvalid(const std::vector<const char*>& args) {
    try {
        (void)parseArgs(args);
        return false;
    } catch (const std::invalid_argument&) {
        return true;
    }
}
}  // namespace

TEST_CASE("parseServerConfig uses defaults", "[server][config]") {
    std::vector<const char*> args{"lumi_server"};

    auto config = parseArgs(args);

    REQUIRE(config.databasePath == "./data/db/projection.db");
    REQUIRE(config.httpPort == 8080);
    REQUIRE(config.rendererHost == "127.0.0.1");
    REQUIRE(config.rendererPort == 5050);
}

TEST_CASE("parseServerConfig accepts overrides", "[server][config]") {
    std::vector<const char*> args{"lumi_server", "--db", "/tmp/projection.db", "--port", "5050",
                                   "--renderer-host", "192.168.0.10", "--renderer-port", "9090"};

    auto config = parseArgs(args);

    REQUIRE(config.databasePath == "/tmp/projection.db");
    REQUIRE(config.httpPort == 5050);
    REQUIRE(config.rendererHost == "192.168.0.10");
    REQUIRE(config.rendererPort == 9090);
}

TEST_CASE("parseServerConfig accepts inline values", "[server][config]") {
    std::vector<const char*> args{"lumi_server", "--db=/opt/app.db", "--port=9090",
                                   "--renderer-host=example.com", "--renderer-port=6060"};

    auto config = parseArgs(args);

    REQUIRE(config.databasePath == "/opt/app.db");
    REQUIRE(config.httpPort == 9090);
    REQUIRE(config.rendererHost == "example.com");
    REQUIRE(config.rendererPort == 6060);
}

TEST_CASE("parseServerConfig rejects missing values", "[server][config]") {
    std::vector<const char*> args{"lumi_server", "--db"};

    REQUIRE(throwsInvalid(args));
}

TEST_CASE("parseServerConfig rejects invalid ports", "[server][config]") {
    std::vector<const char*> args{"lumi_server", "--port", "-1"};

    REQUIRE(throwsInvalid(args));
}

TEST_CASE("parseServerConfig rejects unknown options", "[server][config]") {
    std::vector<const char*> args{"lumi_server", "--unknown"};

    REQUIRE(throwsInvalid(args));
}

}  // namespace projection::server
