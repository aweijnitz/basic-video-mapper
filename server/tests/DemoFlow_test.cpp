#include "db/SchemaMigrations.h"
#include "db/SqliteConnection.h"
#include "http/HttpServer.h"
#include "projection/core/RendererProtocol.h"
#include "renderer/RendererRegistry.h"
#include "repo/FeedRepository.h"
#include "repo/SceneRepository.h"
#include "repo/CueRepository.h"
#include "repo/ProjectRepository.h"

#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <httplib.h>
#include <mutex>
#include <netinet/in.h>
#include <nlohmann/json.hpp>
#include <sys/socket.h>
#include <thread>
#include <unordered_set>
#include <unistd.h>

namespace projection::server {
namespace {

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

std::string tempDbPath(const std::string& name) {
    auto path = std::filesystem::temp_directory_path() / name;
    std::filesystem::remove(path);
    return path.string();
}

std::unique_ptr<httplib::Client> makeClient(int port) {
    auto client = std::make_unique<httplib::Client>("127.0.0.1", port);
    client->set_connection_timeout(0, 200000);  // 200ms
    client->set_read_timeout(1, 0);
    client->set_write_timeout(1, 0);
    return client;
}

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

    std::vector<core::RendererMessage> messages() const {
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

        core::RendererMessage hello{};
        hello.type = core::RendererMessageType::Hello;
        hello.commandId = "cmd-hello";
        hello.hello = core::HelloMessage{"0.1", "renderer", name_};
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
            core::RendererMessage message = json.get<core::RendererMessage>();

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

            core::RendererMessage ack{};
            ack.type = core::RendererMessageType::Ack;
            ack.commandId = message.commandId;
            ack.ack = core::AckMessage{message.commandId};

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
    std::vector<core::RendererMessage> messages_;
};

class ServerRunner {
public:
    ServerRunner(http::HttpServer& server, int port) : server_(server) { thread_ = std::thread([this, port] { server_.start(port); }); }

    ~ServerRunner() {
        if (thread_.joinable()) {
            server_.stop();
            thread_.join();
        }
    }

private:
    http::HttpServer& server_;
    std::thread thread_;
};

struct DemoHttpContext {
    db::SqliteConnection connection;
    repo::FeedRepository feedRepo;
    repo::SceneRepository sceneRepo;
    repo::CueRepository cueRepo;
    repo::ProjectRepository projectRepo;
    std::shared_ptr<renderer::RendererRegistry> rendererRegistry;
    http::HttpServer httpServer;

    DemoHttpContext(const std::string& dbPath, std::shared_ptr<renderer::RendererRegistry> registry)
        : feedRepo(connection),
          sceneRepo(connection),
          cueRepo(connection),
          projectRepo(connection),
          rendererRegistry(std::move(registry)),
          httpServer(feedRepo, sceneRepo, cueRepo, projectRepo, rendererRegistry) {
        connection.open(dbPath);
        db::SchemaMigrations::applyMigrations(connection);
    }
};

bool waitForServer(httplib::Client& client, http::HttpServer& server) {
    for (int attempt = 0; attempt < 100; ++attempt) {
        if (server.isRunning()) {
            if (auto res = client.Get("/feeds")) {
                (void)res;
                return true;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return false;
}

bool waitForRegistry(renderer::RendererRegistry& registry) {
    for (int attempt = 0; attempt < 100; ++attempt) {
        if (registry.port() != 0) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

}  // namespace

TEST_CASE("Two video demo endpoint seeds feeds, scene, and renderer", "[http][demo]") {
    const auto rendererPort = reservePort();
    auto registry = std::make_shared<renderer::RendererRegistry>();
    registry->start(rendererPort);
    REQUIRE(waitForRegistry(*registry));
    FakeRendererClient fakeRenderer("renderer-main", rendererPort);
    REQUIRE(fakeRenderer.waitUntilReady());

    const auto httpPort = reservePort();
    const auto dbPath = tempDbPath("two_video_demo.db");
    DemoHttpContext ctx(dbPath, registry);

    ServerRunner runner(ctx.httpServer, httpPort);
    auto httpClient = makeClient(httpPort);
    REQUIRE(waitForServer(*httpClient, ctx.httpServer));

    auto res = httpClient->Post("/demo/two-video-test", "{}", "application/json");
    REQUIRE(res != nullptr);
    REQUIRE(res->status == 200);

    auto body = nlohmann::json::parse(res->body);
    REQUIRE(body.contains("sceneId"));
    REQUIRE(body.contains("feedIds"));
    REQUIRE(body["feedIds"].is_array());
    REQUIRE(body["feedIds"].size() == 2);

    const auto sceneId = core::SceneId{body["sceneId"].get<std::string>()};
    auto feeds = ctx.feedRepo.listFeeds();
    REQUIRE(feeds.size() == 2);
    REQUIRE(feeds[0].getName() == "Demo Clip A" || feeds[1].getName() == "Demo Clip A");

    auto scene = ctx.sceneRepo.findSceneById(sceneId);
    REQUIRE(scene.has_value());
    REQUIRE(scene->getSurfaces().size() == 2);
    std::unordered_set<std::string> surfaceFeedIds;
    for (const auto& surface : scene->getSurfaces()) {
        surfaceFeedIds.insert(surface.getFeedId().value);
    }
    REQUIRE(surfaceFeedIds.size() == 2);

    REQUIRE(fakeRenderer.waitForMessages(1));
    auto messages = fakeRenderer.messages();
    REQUIRE(messages.front().type == core::RendererMessageType::LoadSceneDefinition);
    REQUIRE(messages.front().loadSceneDefinition.has_value());
    const auto& payload = *messages.front().loadSceneDefinition;
    REQUIRE(payload.scene.getId().value == sceneId.value);
    REQUIRE(payload.feeds.size() == 2);

    std::filesystem::remove(dbPath);
}

}  // namespace projection::server
