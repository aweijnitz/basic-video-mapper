#include "renderer/RendererClient.h"

#include <catch2/catch_test_macros.hpp>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

#include <nlohmann/json.hpp>

#include "projection/core/RendererProtocol.h"

using projection::core::RendererMessage;
using projection::core::RendererMessageType;

namespace projection::server::renderer {
namespace {

class TestServer {
public:
    TestServer() {
        listener_ = ::socket(AF_INET, SOCK_STREAM, 0);
        REQUIRE(listener_ >= 0);

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons(0);

        REQUIRE(::bind(listener_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);

        socklen_t len = sizeof(addr);
        REQUIRE(::getsockname(listener_, reinterpret_cast<sockaddr*>(&addr), &len) == 0);
        port_ = ntohs(addr.sin_port);

        REQUIRE(::listen(listener_, 1) == 0);

        serverThread_ = std::thread([this] {
            sockaddr_in clientAddr{};
            socklen_t clientLen = sizeof(clientAddr);
            clientSocket_ = ::accept(listener_, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
            if (clientSocket_ >= 0) {
                readLoop();
            }
        });
    }

    ~TestServer() {
        if (clientSocket_ >= 0) {
            ::close(clientSocket_);
        }
        if (listener_ >= 0) {
            ::shutdown(listener_, SHUT_RDWR);
            ::close(listener_);
        }
        if (serverThread_.joinable()) {
            serverThread_.join();
        }
    }

    int port() const { return port_; }

    std::string receivedLine() const { return receivedLine_; }

private:
    void readLoop() {
        std::string buffer;
        char chunk[256];
        while (true) {
            ssize_t count = ::recv(clientSocket_, chunk, sizeof(chunk), 0);
            if (count <= 0) {
                return;
            }
            buffer.append(chunk, static_cast<size_t>(count));
            auto pos = buffer.find('\n');
            if (pos != std::string::npos) {
                receivedLine_ = buffer.substr(0, pos);

                RendererMessage ack{};
                ack.type = RendererMessageType::Ack;
                ack.commandId = "123";
                ack.ack = projection::core::AckMessage{"123"};
                auto json = nlohmann::json(ack).dump();
                json.push_back('\n');
                ::send(clientSocket_, json.c_str(), json.size(), 0);
                return;
            }
        }
    }

    int listener_{-1};
    int clientSocket_{-1};
    int port_{0};
    std::thread serverThread_;
    std::string receivedLine_;
};

}  // namespace

TEST_CASE("RendererClient connects to server", "[renderer][client]") {
    TestServer server;
    RendererClient client{"127.0.0.1", server.port()};

    client.connect();
    client.disconnect();
}

TEST_CASE("RendererClient sends and receives JSON messages", "[renderer][client]") {
    TestServer server;
    RendererClient client{"127.0.0.1", server.port()};
    client.connect();

    RendererMessage msg{};
    msg.type = RendererMessageType::Hello;
    msg.commandId = "123";
    msg.hello = projection::core::HelloMessage{"1.0", "server"};

    client.sendMessage(msg);

    auto response = client.receiveMessage();
    REQUIRE(response.type == RendererMessageType::Ack);
    REQUIRE(response.commandId == "123");
    REQUIRE(response.ack.has_value());
    REQUIRE(response.ack->commandId == "123");

    nlohmann::json sentJson = nlohmann::json::parse(server.receivedLine());
    auto parsed = sentJson.get<RendererMessage>();
    REQUIRE(parsed == msg);

    client.disconnect();
}

TEST_CASE("RendererClient connect fails on unavailable port", "[renderer][client]") {
    RendererClient client{"127.0.0.1", 59999};
    bool threw = false;
    try {
        client.connect();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    REQUIRE(threw);
}

}  // namespace projection::server::renderer

