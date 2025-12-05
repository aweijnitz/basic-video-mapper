#include "ServerApp.h"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>

namespace projection::server {

namespace {
std::string tempDbPath(const std::string& name) {
    return (std::filesystem::temp_directory_path() / name).string();
}
}  // namespace

TEST_CASE("ServerApp constructs with configuration", "[server][app]") {
    ServerConfig config{tempDbPath("server_app_construct.db"), 8080};
    bool threw = false;
    try {
        ServerApp app{config};
        (void)app;
    } catch (...) {
        threw = true;
    }

    REQUIRE(!threw);
}

TEST_CASE("ServerApp run returns status code", "[server][app]") {
    ServerConfig config{tempDbPath("server_app_run.db"), 8080};
    ServerApp app{config};
    auto status = app.run();
    REQUIRE(status == 0);
}

}  // namespace projection::server
