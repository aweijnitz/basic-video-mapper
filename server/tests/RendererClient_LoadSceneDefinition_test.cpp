#include "renderer/RendererClient.h"

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

#include <mutex>
#include <condition_variable>
#include <optional>
#include <vector>

#include <nlohmann/json.hpp>

#include "projection/core/RendererProtocol.h"
#include "projection/core/Serialization.h"

using projection::core::Feed;
using projection::core::FeedId;
using projection::core::FeedType;
using projection::core::RendererMessage;
using projection::core::RendererMessageType;
using projection::core::Scene;
using projection::core::SceneId;
using projection::core::Surface;
using projection::core::SurfaceId;
using projection::core::Vec2;

namespace projection::server::renderer {
namespace {

class LoadSceneDefinitionServer {
public:
    LoadSceneDefinitionServer() {
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

    ~LoadSceneDefinitionServer() {
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

    bool waitForMessage(std::chrono::milliseconds timeout = std::chrono::milliseconds(1000)) {
        std::unique_lock<std::mutex> lock(mutex_);
        return messageReceivedCv_.wait_for(lock, timeout, [this] { return message_.has_value(); });
    }

    RendererMessage message() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return *message_;
    }

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
                std::string line = buffer.substr(0, pos);
                auto parsed = nlohmann::json::parse(line).get<RendererMessage>();
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    message_ = parsed;
                }
                messageReceivedCv_.notify_all();
                return;
            }
        }
    }

    int listener_{-1};
    int clientSocket_{-1};
    int port_{0};
    std::thread serverThread_;

    mutable std::mutex mutex_;
    std::condition_variable messageReceivedCv_;
    std::optional<RendererMessage> message_;
};

Scene buildScene(const FeedId& feedA, const FeedId& feedB) {
    std::vector<Vec2> quad{{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};
    std::vector<Surface> surfaces{Surface(SurfaceId{"surface-1"}, "Left", quad, feedA),
                                  Surface(SurfaceId{"surface-2"}, "Right", quad, feedB)};
    return Scene(SceneId{"scene-test"}, "Test", "Definition", surfaces);
}

std::vector<Feed> buildFeeds() {
    return {Feed(FeedId{"feed-a"}, "Feed A", FeedType::VideoFile, R"({"filePath":"a.mp4"})"),
            Feed(FeedId{"feed-b"}, "Feed B", FeedType::VideoFile, R"({"filePath":"b.mp4"})")};
}

}  // namespace

TEST_CASE("RendererClient sends LoadSceneDefinition", "[renderer][client]") {
    LoadSceneDefinitionServer server;
    RendererClient client{"127.0.0.1", server.port()};
    client.connect();

    auto feeds = buildFeeds();
    auto scene = buildScene(feeds[0].getId(), feeds[1].getId());

    client.sendLoadSceneDefinition(scene, feeds);

    REQUIRE(server.waitForMessage());
    auto message = server.message();

    REQUIRE(message.type == RendererMessageType::LoadSceneDefinition);
    REQUIRE(message.loadSceneDefinition.has_value());
    REQUIRE(message.loadSceneDefinition->scene.getId().value == scene.getId().value);
    REQUIRE(message.loadSceneDefinition->feeds.size() == feeds.size());
    REQUIRE(message.loadSceneDefinition->feeds[0].getId().value == feeds[0].getId().value);
    REQUIRE(message.loadSceneDefinition->feeds[1].getId().value == feeds[1].getId().value);
}

}  // namespace projection::server::renderer
