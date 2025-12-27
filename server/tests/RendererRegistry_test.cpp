#include "renderer/RendererRegistry.h"

#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <netinet/in.h>
#include <nlohmann/json.hpp>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

using projection::core::RendererMessage;
using projection::core::RendererMessageType;

namespace projection::server::renderer {
namespace {
class FakeRendererClient {
public:
    FakeRendererClient(std::string name, int port) : name_(std::move(name)), port_(port) {
        thread_ = std::thread([this] { run(); });
    }

    ~FakeRendererClient() {
        stop_ = true;
        if (socketFd_ >= 0) {
            ::shutdown(socketFd_, SHUT_RDWR);
            ::close(socketFd_);
        }
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    bool waitUntilReady(std::chrono::milliseconds timeout = std::chrono::milliseconds(1000)) {
        std::unique_lock<std::mutex> lock(mutex_);
        return readyCv_.wait_for(lock, timeout, [this] { return ready_; });
    }

    bool waitForMessages(size_t expectedCount, std::chrono::milliseconds timeout = std::chrono::milliseconds(2000)) {
        auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (messages_.size() >= expectedCount) {
                    return true;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        return false;
    }

    std::vector<RendererMessage> messages() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return messages_;
    }

private:
    void run() {
        socketFd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (socketFd_ < 0) {
            return;
        }

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons(static_cast<uint16_t>(port_));

        if (::connect(socketFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            return;
        }

        RendererMessage hello{};
        hello.type = RendererMessageType::Hello;
        hello.commandId = "cmd-hello";
        hello.hello = projection::core::HelloMessage{"0.1", "renderer", name_};
        std::string payload = nlohmann::json(hello).dump() + "\n";
        ::send(socketFd_, payload.c_str(), payload.size(), 0);

        std::string buffer;
        char chunk[256];
        while (!stop_) {
            ssize_t received = ::recv(socketFd_, chunk, sizeof(chunk), 0);
            if (received <= 0) {
                break;
            }
            buffer.append(chunk, static_cast<size_t>(received));
            auto newlinePos = buffer.find('\n');
            if (newlinePos == std::string::npos) {
                continue;
            }

            std::string line = buffer.substr(0, newlinePos);
            buffer.erase(0, newlinePos + 1);

            auto json = nlohmann::json::parse(line);
            RendererMessage message = json.get<RendererMessage>();

            if (!ready_) {
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    ready_ = true;
                }
                readyCv_.notify_all();
                continue;
            }

            {
                std::lock_guard<std::mutex> lock(mutex_);
                messages_.push_back(message);
            }

            RendererMessage ack{};
            ack.type = RendererMessageType::Ack;
            ack.commandId = message.commandId;
            ack.ack = projection::core::AckMessage{message.commandId};
            std::string response = nlohmann::json(ack).dump() + "\n";
            ::send(socketFd_, response.c_str(), response.size(), 0);
        }
    }

    std::string name_;
    int port_;
    int socketFd_{-1};
    std::thread thread_;
    std::atomic<bool> stop_{false};

    mutable std::mutex mutex_;
    std::condition_variable readyCv_;
    bool ready_{false};
    std::vector<RendererMessage> messages_;
};

RendererMessage handshakeOnce(const std::string& name, int port) {
    int socketFd = ::socket(AF_INET, SOCK_STREAM, 0);
    REQUIRE(socketFd >= 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(static_cast<uint16_t>(port));

    REQUIRE(::connect(socketFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);

    RendererMessage hello{};
    hello.type = RendererMessageType::Hello;
    hello.commandId = "cmd-hello";
    hello.hello = projection::core::HelloMessage{"0.1", "renderer", name};
    std::string payload = nlohmann::json(hello).dump() + "\n";
    ::send(socketFd, payload.c_str(), payload.size(), 0);

    std::string buffer;
    char chunk[256];
    while (true) {
        ssize_t received = ::recv(socketFd, chunk, sizeof(chunk), 0);
        REQUIRE(received > 0);
        buffer.append(chunk, static_cast<size_t>(received));
        auto newlinePos = buffer.find('\n');
        if (newlinePos == std::string::npos) {
            continue;
        }

        std::string line = buffer.substr(0, newlinePos);
        auto message = nlohmann::json::parse(line).get<RendererMessage>();
        ::close(socketFd);
        return message;
    }
}

int waitForPort(RendererRegistry& registry) {
    for (int i = 0; i < 100; ++i) {
        int port = registry.port();
        if (port != 0) {
            return port;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return 0;
}
}  // namespace

TEST_CASE("RendererRegistry accepts renderer and broadcasts messages", "[renderer][registry]") {
    RendererRegistry registry;
    registry.start(0);
    const int port = waitForPort(registry);
    REQUIRE(port != 0);

    FakeRendererClient renderer("alpha", port);
    REQUIRE(renderer.waitUntilReady());

    RendererMessage message{};
    message.type = RendererMessageType::LoadScene;
    message.commandId = "cmd-load";
    message.loadScene = projection::core::LoadSceneMessage{projection::core::SceneId{"scene-1"}};

    REQUIRE(registry.broadcastMessage(message) == 1);
    REQUIRE(renderer.waitForMessages(1));

    auto messages = renderer.messages();
    REQUIRE(messages.front().type == RendererMessageType::LoadScene);
    REQUIRE(messages.front().loadScene.has_value());
    REQUIRE(messages.front().loadScene->sceneId.value == "scene-1");

    registry.stop();
}

TEST_CASE("RendererRegistry rejects duplicate renderer names", "[renderer][registry]") {
    RendererRegistry registry;
    registry.start(0);
    const int port = waitForPort(registry);
    REQUIRE(port != 0);

    FakeRendererClient renderer("duplicate", port);
    REQUIRE(renderer.waitUntilReady());

    auto second = handshakeOnce("duplicate", port);
    REQUIRE(second.type == RendererMessageType::Error);
    REQUIRE(second.error.has_value());
    REQUIRE(second.error->message == "Renderer name already in use");

    registry.stop();
}
}  // namespace projection::server::renderer
