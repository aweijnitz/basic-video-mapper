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

class DummyRendererServer {
public:
    explicit DummyRendererServer(int port) {
        listener_ = ::socket(AF_INET, SOCK_STREAM, 0);
        REQUIRE(listener_ >= 0);

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons(static_cast<uint16_t>(port));

        REQUIRE(::bind(listener_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
        REQUIRE(::listen(listener_, 1) == 0);

        thread_ = std::thread([this] {
            sockaddr_in clientAddr{};
            socklen_t clientLen = sizeof(clientAddr);
            int client = ::accept(listener_, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
            if (client >= 0) {
                char buffer[1];
                (void)::recv(client, buffer, sizeof(buffer), 0);
                ::close(client);
            }
        });
    }

    ~DummyRendererServer() {
        if (listener_ >= 0) {
            ::shutdown(listener_, SHUT_RDWR);
            ::close(listener_);
        }
        if (thread_.joinable()) {
            thread_.join();
        }
    }

private:
    int listener_{-1};
    std::thread thread_;
};
}  // namespace

TEST_CASE("ServerApp constructs with configuration", "[server][app]") {
    ServerConfig config{tempDbPath("server_app_construct.db"), 8080, "127.0.0.1", 5555};
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
    int httpPort = reservePort();
    int rendererPort = reservePort();
    DummyRendererServer renderer(rendererPort);
    ServerConfig config{tempDbPath("server_app_run.db"), httpPort, "127.0.0.1", rendererPort};
    ServerApp app{config};

    int status = -1;
    std::thread serverThread([&] { status = app.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    app.stop();
    serverThread.join();

    REQUIRE(status == 0);
}

}  // namespace projection::server
