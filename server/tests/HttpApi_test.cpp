#include "ServerApp.h"
#include "db/SchemaMigrations.h"
#include "db/SqliteConnection.h"
#include "http/HttpServer.h"
#include "repo/FeedRepository.h"
#include "repo/SceneRepository.h"

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <httplib.h>
#include <memory>
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

std::string tempDbPath(const std::string& name) {
    return (std::filesystem::temp_directory_path() / name).string();
}

struct TestServerContext {
    db::SqliteConnection connection;
    repo::FeedRepository feedRepo;
    repo::SceneRepository sceneRepo;
    http::HttpServer httpServer;

    explicit TestServerContext(const std::string& dbPath)
        : feedRepo(connection), sceneRepo(connection), httpServer(feedRepo, sceneRepo) {
        connection.open(dbPath);
        db::SchemaMigrations::applyMigrations(connection);
    }
};

class ServerRunner {
public:
    ServerRunner(http::HttpServer& server, int port) : server_(server) {
        thread_ = std::thread([this, port] {
            try {
                server_.start(port);
            } catch (...) {
                startError_ = std::current_exception();
            }
        });
    }

    ~ServerRunner() {
        if (thread_.joinable()) {
            server_.stop();
            thread_.join();
        }
    }

    bool hadError() const { return static_cast<bool>(startError_); }
    std::exception_ptr error() const { return startError_; }

private:
    http::HttpServer& server_;
    std::thread thread_;
    std::exception_ptr startError_;
};

std::unique_ptr<httplib::Client> makeClient(int port) {
    auto client = std::make_unique<httplib::Client>("127.0.0.1", port);
    client->set_connection_timeout(0, 200000);  // 200ms
    client->set_read_timeout(1, 0);
    client->set_write_timeout(1, 0);
    return client;
}

bool waitForServer(httplib::Client& client, http::HttpServer& server, const ServerRunner& runner) {
    for (int attempt = 0; attempt < 100; ++attempt) {
        if (runner.hadError()) {
            std::rethrow_exception(runner.error());
        }
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

std::string feedBody() {
    nlohmann::json feed{{"id", "1"}, {"name", "Camera"}, {"type", "Camera"}, {"configJson", "{}"}};
    return feed.dump();
}

std::string sceneBody() {
    nlohmann::json scene{{"id", "1"}, {"name", "Main"}, {"description", "Test scene"}, {"surfaces", nlohmann::json::array()}};
    return scene.dump();
}

}  // namespace

TEST_CASE("HTTP API can create and list feeds", "[http][integration]") {
    auto dbPath = tempDbPath("http_api_feeds.db");
    TestServerContext ctx(dbPath);
    const auto port = reservePort();
    ServerRunner runner(ctx.httpServer, port);

    auto client = makeClient(port);
    REQUIRE(waitForServer(*client, ctx.httpServer, runner));

    auto postRes = client->Post("/feeds", feedBody(), "application/json");
    REQUIRE(postRes != nullptr);
    if (postRes->status != 201) {
        throw std::runtime_error("Feed POST failed with status " + std::to_string(postRes->status) +
                                 " body: " + postRes->body);
    }

    auto getRes = client->Get("/feeds");
    REQUIRE(getRes != nullptr);
    REQUIRE(getRes->status == 200);

    auto bodyJson = nlohmann::json::parse(getRes->body);
    REQUIRE(bodyJson.is_array());
    REQUIRE(bodyJson.size() == 1);
    REQUIRE(bodyJson[0]["id"] == "1");

    std::filesystem::remove(dbPath);
}

TEST_CASE("HTTP API can create and list scenes", "[http][integration]") {
    auto dbPath = tempDbPath("http_api_scenes.db");
    TestServerContext ctx(dbPath);
    const auto port = reservePort();
    ServerRunner runner(ctx.httpServer, port);

    auto client = makeClient(port);
    REQUIRE(waitForServer(*client, ctx.httpServer, runner));

    auto postRes = client->Post("/scenes", sceneBody(), "application/json");
    REQUIRE(postRes != nullptr);
    if (postRes->status != 201) {
        throw std::runtime_error("Scene POST failed with status " + std::to_string(postRes->status) +
                                 " body: " + postRes->body);
    }

    auto getRes = client->Get("/scenes");
    REQUIRE(getRes != nullptr);
    REQUIRE(getRes->status == 200);

    auto bodyJson = nlohmann::json::parse(getRes->body);
    REQUIRE(bodyJson.is_array());
    REQUIRE(bodyJson.size() == 1);
    REQUIRE(bodyJson[0]["id"] == "1");

    std::filesystem::remove(dbPath);
}

TEST_CASE("HTTP API returns 400 on invalid JSON", "[http][integration]") {
    auto dbPath = tempDbPath("http_api_invalid_json.db");
    TestServerContext ctx(dbPath);
    const auto port = reservePort();
    ServerRunner runner(ctx.httpServer, port);

    auto client = makeClient(port);
    REQUIRE(waitForServer(*client, ctx.httpServer, runner));

    auto postRes = client->Post("/feeds", "not-json", "application/json");
    REQUIRE(postRes != nullptr);
    REQUIRE(postRes->status == 400);

    std::filesystem::remove(dbPath);
}

TEST_CASE("HTTP API validates required fields", "[http][integration]") {
    auto dbPath = tempDbPath("http_api_missing_fields.db");
    TestServerContext ctx(dbPath);
    const auto port = reservePort();
    ServerRunner runner(ctx.httpServer, port);

    auto client = makeClient(port);
    REQUIRE(waitForServer(*client, ctx.httpServer, runner));
    nlohmann::json badFeed{{"name", "Missing id"}};
    auto postRes = client->Post("/feeds", badFeed.dump(), "application/json");
    REQUIRE(postRes != nullptr);
    REQUIRE(postRes->status == 400);

    std::filesystem::remove(dbPath);
}

}  // namespace projection::server
