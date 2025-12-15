#include "db/SchemaMigrations.h"
#include "db/SqliteConnection.h"
#include "http/HttpServer.h"
#include "projection/core/RendererProtocol.h"
#include "renderer/RendererClient.h"
#include "repo/FeedRepository.h"
#include "repo/SceneRepository.h"
#include "repo/CueRepository.h"

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <condition_variable>
#include <atomic>
#include <filesystem>
#include <httplib.h>
#include <mutex>
#include <vector>
#include <netinet/in.h>
#include <nlohmann/json.hpp>
#include <sys/socket.h>
#include <thread>
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

std::string tempDbPath(const std::string& name) { return (std::filesystem::temp_directory_path() / name).string(); }

std::unique_ptr<httplib::Client> makeClient(int port) {
    auto client = std::make_unique<httplib::Client>("127.0.0.1", port);
    client->set_connection_timeout(0, 200000);  // 200ms
    client->set_read_timeout(1, 0);
    client->set_write_timeout(1, 0);
    return client;
}

class FakeRendererServer {
public:
    explicit FakeRendererServer(int port) : port_(port) { thread_ = std::thread([this] { run(); }); }

    ~FakeRendererServer() {
        stop_ = true;
        if (clientFd_ >= 0) {
            ::shutdown(clientFd_, SHUT_RDWR);
            ::close(clientFd_);
        }
        if (serverFd_ >= 0) {
            ::shutdown(serverFd_, SHUT_RDWR);
            ::close(serverFd_);
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
        serverFd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (serverFd_ < 0) {
            return;
        }

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons(static_cast<uint16_t>(port_));

        if (::bind(serverFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            return;
        }

        if (::listen(serverFd_, 1) != 0) {
            return;
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            ready_ = true;
        }
        readyCv_.notify_all();

        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        clientFd_ = ::accept(serverFd_, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
        if (clientFd_ < 0) {
            return;
        }

        std::string buffer;
        char chunk[256];
        while (!stop_) {
            ssize_t received = ::recv(clientFd_, chunk, sizeof(chunk), 0);
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

            {
                std::lock_guard<std::mutex> lock(mutex_);
                messages_.push_back(message);
            }

            core::RendererMessage ack{};
            ack.type = core::RendererMessageType::Ack;
            ack.commandId = message.commandId;
            ack.ack = core::AckMessage{message.commandId};

            std::string response = nlohmann::json(ack).dump() + "\n";
            ::send(clientFd_, response.c_str(), response.size(), 0);
        }
    }

    int port_;
    int serverFd_{-1};
    int clientFd_{-1};
    std::thread thread_;
    std::atomic<bool> stop_{false};

    mutable std::mutex mutex_;
    std::condition_variable readyCv_;
    bool ready_{false};
    std::vector<core::RendererMessage> messages_;
};

class ServerRunner {
public:
    ServerRunner(http::HttpServer& server, int port) : server_(server) {
        thread_ = std::thread([this, port] { server_.start(port); });
    }

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

struct RendererHttpContext {
    db::SqliteConnection connection;
    repo::FeedRepository feedRepo;
    repo::SceneRepository sceneRepo;
    repo::CueRepository cueRepo;
    std::shared_ptr<renderer::RendererClient> rendererClient;
    http::HttpServer httpServer;

    RendererHttpContext(const std::string& dbPath, std::shared_ptr<renderer::RendererClient> client)
        : feedRepo(connection),
          sceneRepo(connection),
          cueRepo(connection),
          rendererClient(std::move(client)),
          httpServer(feedRepo, sceneRepo, cueRepo, rendererClient) {
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

}  // namespace

TEST_CASE("Renderer ping endpoint talks to renderer", "[http][renderer]") {
    const auto rendererPort = reservePort();
    FakeRendererServer fakeRenderer(rendererPort);
    REQUIRE(fakeRenderer.waitUntilReady());

    auto rendererClient = std::make_shared<renderer::RendererClient>("127.0.0.1", rendererPort);
    const auto httpPort = reservePort();
    const auto dbPath = tempDbPath("renderer_ping.db");
    RendererHttpContext ctx(dbPath, rendererClient);

    ServerRunner runner(ctx.httpServer, httpPort);
    auto httpClient = makeClient(httpPort);
    REQUIRE(waitForServer(*httpClient, ctx.httpServer));

    rendererClient->connect();

    auto res = httpClient->Post("/renderer/ping", "{}", "application/json");
    REQUIRE(res != nullptr);
    REQUIRE(res->status == 200);
    REQUIRE(fakeRenderer.waitForMessages(1));

    auto messages = fakeRenderer.messages();
    REQUIRE(messages.front().type == core::RendererMessageType::Hello);
    REQUIRE(messages.front().hello.has_value());
    REQUIRE(messages.front().hello->role == "server");

    std::filesystem::remove(dbPath);
}

TEST_CASE("LoadScene endpoint validates and forwards to renderer", "[http][renderer]") {
    const auto rendererPort = reservePort();
    FakeRendererServer fakeRenderer(rendererPort);
    REQUIRE(fakeRenderer.waitUntilReady());

    auto rendererClient = std::make_shared<renderer::RendererClient>("127.0.0.1", rendererPort);
    const auto httpPort = reservePort();
    const auto dbPath = tempDbPath("renderer_load_scene.db");
    RendererHttpContext ctx(dbPath, rendererClient);

    core::Feed feedA(core::FeedId{}, "Feed A", core::FeedType::VideoFile, R"({"filePath":"a.mp4"})");
    core::Feed feedB(core::FeedId{}, "Feed B", core::FeedType::VideoFile, R"({"filePath":"b.mp4"})");
    feedA = ctx.feedRepo.createFeed(feedA);
    feedB = ctx.feedRepo.createFeed(feedB);

    std::vector<core::Vec2> quad{{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};
    std::vector<core::Surface> surfaces{
        core::Surface(core::SurfaceId{"integration-surface-1"}, "One", quad, feedA.getId()),
        core::Surface(core::SurfaceId{"integration-surface-2"}, "Two", quad, feedB.getId())};
    core::Scene scene(core::SceneId{}, "Test", "Renderer scene", surfaces);
    scene = ctx.sceneRepo.createScene(scene);

    ServerRunner runner(ctx.httpServer, httpPort);
    auto httpClient = makeClient(httpPort);
    REQUIRE(waitForServer(*httpClient, ctx.httpServer));

    rendererClient->connect();

    nlohmann::json requestPayload{{"sceneId", scene.getId().value}};
    auto res = httpClient->Post("/renderer/loadScene", requestPayload.dump(), "application/json");
    REQUIRE(res != nullptr);
    REQUIRE(res->status == 200);
    REQUIRE(fakeRenderer.waitForMessages(1));

    auto messages = fakeRenderer.messages();
    REQUIRE(messages.front().type == core::RendererMessageType::LoadSceneDefinition);
    REQUIRE(messages.front().loadSceneDefinition.has_value());
    const auto& messagePayload = *messages.front().loadSceneDefinition;
    REQUIRE(messagePayload.scene.getId().value == scene.getId().value);
    REQUIRE(messagePayload.scene.getSurfaces().size() == 2);
    REQUIRE(messagePayload.scene.getSurfaces()[0].getFeedId().value == feedA.getId().value);
    REQUIRE(messagePayload.scene.getSurfaces()[1].getFeedId().value == feedB.getId().value);
    REQUIRE(messagePayload.feeds.size() == 2);
    REQUIRE(messagePayload.feeds[0].getId().value == feedA.getId().value);
    REQUIRE(messagePayload.feeds[1].getId().value == feedB.getId().value);

    std::filesystem::remove(dbPath);
}

}  // namespace projection::server
