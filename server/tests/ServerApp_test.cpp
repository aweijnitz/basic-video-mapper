#include "ServerApp.h"

#include <catch2/catch_test_macros.hpp>

namespace projection::server {

TEST_CASE("ServerApp constructs with configuration", "[server][app]") {
    ServerConfig config{"./data/db/projection.db", 8080};
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
    ServerConfig config{"./data/db/projection.db", 8080};
    ServerApp app{config};
    auto status = app.run();
    REQUIRE(status == 0);
}

}  // namespace projection::server
