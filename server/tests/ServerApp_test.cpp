#include "ServerApp.h"

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace projection::server {

namespace {
std::string tempDbPath(const std::string& name) {
    return (std::filesystem::temp_directory_path() / name).string();
}

int reservePort() {
    int sock = ::socket(AF_INET, SOCK_STREAM, 0);
    REQUIRE(sock >= 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;

    REQUIRE(::bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);

    socklen_t len = sizeof(addr);
    REQUIRE(::getsockname(sock, reinterpret_cast<sockaddr*>(&addr), &len) == 0);
    int port = ntohs(addr.sin_port);
    ::close(sock);
    return port;
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
    ServerConfig config{tempDbPath("server_app_run.db"), reservePort()};
    ServerApp app{config};

    int status = -1;
    std::thread serverThread([&] { status = app.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    app.stop();
    serverThread.join();

    REQUIRE(status == 0);
}

}  // namespace projection::server
