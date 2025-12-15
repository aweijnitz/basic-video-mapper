#include "ServerApp.h"
#include "db/SchemaMigrations.h"
#include "db/SqliteConnection.h"
#include "http/HttpServer.h"
#include "repo/FeedRepository.h"
#include "repo/SceneRepository.h"
#include "repo/CueRepository.h"
#include "repo/ProjectRepository.h"

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
#include <vector>

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
    repo::CueRepository cueRepo;
    repo::ProjectRepository projectRepo;
    http::HttpServer httpServer;

    explicit TestServerContext(const std::string& dbPath)
        : feedRepo(connection),
          sceneRepo(connection),
          cueRepo(connection),
          projectRepo(connection),
          httpServer(feedRepo, sceneRepo, cueRepo, projectRepo, nullptr) {
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

std::string sceneWithSurfaceBody(const std::string& sceneId, const std::string& feedId) {
    nlohmann::json scene{
        {"id", sceneId},
        {"name", "Main"},
        {"description", "With surface"},
        {"surfaces",
         {{{"id", "s1"},
           {"name", "surf"},
           {"vertices", {{{"x", 0}, {"y", 0}}, {{"x", 1}, {"y", 0}}, {{"x", 0}, {"y", 1}}}},
           {"feedId", feedId},
           {"opacity", 1.0},
           {"brightness", 1.0},
           {"blendMode", "Normal"},
           {"zOrder", 0}}}}};
    return scene.dump();
}

std::string cueBody(const std::string& cueId, const std::string& sceneId, const std::string& surfaceId) {
    nlohmann::json cue{{"id", cueId},
                       {"name", "CueName"},
                       {"sceneId", sceneId},
                       {"surfaceOpacities", {{{"surfaceId", surfaceId}, {"value", 1.0}}}},
                       {"surfaceBrightnesses", {{{"surfaceId", surfaceId}, {"value", 1.0}}}}};
    return cue.dump();
}

std::string projectBody(const std::string& projectId, const std::vector<std::string>& cueOrder,
                        nlohmann::json settings = nlohmann::json::object()) {
    nlohmann::json project{{"id", projectId},
                           {"name", "ProjectName"},
                           {"description", "Project description"},
                           {"cueOrder", cueOrder},
                           {"settings", settings.empty() ? nlohmann::json::object() : settings}};
    return project.dump();
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

TEST_CASE("HTTP API can create, fetch, update, and delete projects", "[http][integration][projects]") {
    auto dbPath = tempDbPath("http_api_projects.db");
    TestServerContext ctx(dbPath);
    const auto port = reservePort();
    ServerRunner runner(ctx.httpServer, port);

    auto client = makeClient(port);
    REQUIRE(waitForServer(*client, ctx.httpServer, runner));

    REQUIRE(client->Post("/feeds", feedBody(), "application/json")->status == 201);
    REQUIRE(client->Post("/scenes", sceneWithSurfaceBody("scene-1", "1"), "application/json")->status == 201);
    REQUIRE(client->Post("/cues", cueBody("cue-1", "scene-1", "s1"), "application/json")->status == 201);

    nlohmann::json settings{{"controllers", {{"fader1", "master"}}}, {"midiChannels", {1, 2}}, {"globalConfig", {}}};
    auto createRes = client->Post("/projects", projectBody("project-1", {"cue-1"}, settings), "application/json");
    REQUIRE(createRes != nullptr);
    REQUIRE(createRes->status == 201);

    auto listRes = client->Get("/projects");
    REQUIRE(listRes != nullptr);
    REQUIRE(listRes->status == 200);
    auto projects = nlohmann::json::parse(listRes->body);
    REQUIRE(projects.is_array());
    REQUIRE(projects.size() == 1);
    REQUIRE(projects[0]["cueOrder"].size() == 1);

    auto getRes = client->Get("/projects/project-1");
    REQUIRE(getRes != nullptr);
    REQUIRE(getRes->status == 200);
    auto projectJson = nlohmann::json::parse(getRes->body);
    REQUIRE(projectJson["settings"]["controllers"]["fader1"] == "master");

    nlohmann::json updatedSettings{{"controllers", {{"knob1", "hue"}}}, {"midiChannels", {3}}, {"globalConfig", {}}};
    auto updatePayload = nlohmann::json{{"id", "ignored"},
                                        {"name", "ProjectName"},
                                        {"description", "Updated"},
                                        {"cueOrder", nlohmann::json::array({"cue-1"})},
                                        {"settings", updatedSettings}};
    auto updateRes = client->Put("/projects/project-1", updatePayload.dump(), "application/json");
    REQUIRE(updateRes != nullptr);
    REQUIRE(updateRes->status == 200);
    auto updatedJson = nlohmann::json::parse(updateRes->body);
    REQUIRE(updatedJson["description"] == "Updated");
    REQUIRE(updatedJson["settings"]["controllers"]["knob1"] == "hue");

    auto deleteRes = client->Delete("/projects/project-1");
    REQUIRE(deleteRes != nullptr);
    REQUIRE(deleteRes->status == 204);

    std::filesystem::remove(dbPath);
}

TEST_CASE("HTTP API rejects projects referencing unknown cues", "[http][integration][projects][validation]") {
    auto dbPath = tempDbPath("http_api_projects_validation.db");
    TestServerContext ctx(dbPath);
    const auto port = reservePort();
    ServerRunner runner(ctx.httpServer, port);

    auto client = makeClient(port);
    REQUIRE(waitForServer(*client, ctx.httpServer, runner));

    auto createRes = client->Post("/projects", projectBody("project-1", {"missing-cue"}), "application/json");
    REQUIRE(createRes != nullptr);
    REQUIRE(createRes->status == 400);

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

TEST_CASE("HTTP API prevents deleting feeds referenced by scenes", "[http][integration]") {
    auto dbPath = tempDbPath("http_api_feed_delete_guard.db");
    TestServerContext ctx(dbPath);
    const auto port = reservePort();
    ServerRunner runner(ctx.httpServer, port);

    auto client = makeClient(port);
    REQUIRE(waitForServer(*client, ctx.httpServer, runner));

    REQUIRE(client->Post("/feeds", feedBody(), "application/json")->status == 201);
    REQUIRE(client->Post("/scenes", sceneWithSurfaceBody("scene-guard", "1"), "application/json")->status == 201);

    auto delRes = client->Delete("/feeds/1");
    REQUIRE(delRes != nullptr);
    REQUIRE(delRes->status == 400);
    REQUIRE(delRes->body.find("referenced by scene") != std::string::npos);
    std::filesystem::remove(dbPath);
}

TEST_CASE("HTTP API prevents deleting scenes referenced by cues", "[http][integration]") {
    auto dbPath = tempDbPath("http_api_scene_delete_guard.db");
    TestServerContext ctx(dbPath);
    const auto port = reservePort();
    ServerRunner runner(ctx.httpServer, port);

    auto client = makeClient(port);
    REQUIRE(waitForServer(*client, ctx.httpServer, runner));

    REQUIRE(client->Post("/feeds", feedBody(), "application/json")->status == 201);
    REQUIRE(client->Post("/scenes", sceneWithSurfaceBody("scene-guard", "1"), "application/json")->status == 201);
    auto cueRes = client->Post("/cues", cueBody("cue-1", "scene-guard", "s1"), "application/json");
    REQUIRE(cueRes != nullptr);
    REQUIRE(cueRes->status == 201);

    auto delRes = client->Delete("/scenes/scene-guard");
    REQUIRE(delRes != nullptr);
    REQUIRE(delRes->status == 400);
    REQUIRE(delRes->body.find("referenced by cue") != std::string::npos);
    std::filesystem::remove(dbPath);
}

TEST_CASE("HTTP API supports cue CRUD", "[http][integration]") {
    auto dbPath = tempDbPath("http_api_cues_crud.db");
    TestServerContext ctx(dbPath);
    const auto port = reservePort();
    ServerRunner runner(ctx.httpServer, port);

    auto client = makeClient(port);
    REQUIRE(waitForServer(*client, ctx.httpServer, runner));

    REQUIRE(client->Post("/feeds", feedBody(), "application/json")->status == 201);
    REQUIRE(client->Post("/scenes", sceneWithSurfaceBody("scene-1", "1"), "application/json")->status == 201);

    auto createRes = client->Post("/cues", cueBody("cue-1", "scene-1", "s1"), "application/json");
    REQUIRE(createRes != nullptr);
    REQUIRE(createRes->status == 201);

    auto listRes = client->Get("/cues");
    REQUIRE(listRes != nullptr);
    REQUIRE(listRes->status == 200);
    auto cues = nlohmann::json::parse(listRes->body);
    REQUIRE(cues.is_array());
    REQUIRE(!cues.empty());

    auto updateBody = cueBody("cue-1", "scene-1", "s1");
    auto updateJson = nlohmann::json::parse(updateBody);
    updateJson["name"] = "UpdatedCue";
    auto updateRes = client->Put("/cues/cue-1", updateJson.dump(), "application/json");
    REQUIRE(updateRes != nullptr);
    REQUIRE(updateRes->status == 200);

    auto deleteRes = client->Delete("/cues/cue-1");
    REQUIRE(deleteRes != nullptr);
    REQUIRE(deleteRes->status == 204);

    std::filesystem::remove(dbPath);
}

TEST_CASE("HTTP API prevents deleting cues referenced by projects", "[http][integration][projects]") {
    auto dbPath = tempDbPath("http_api_project_cue_guard.db");
    TestServerContext ctx(dbPath);
    const auto port = reservePort();
    ServerRunner runner(ctx.httpServer, port);

    auto client = makeClient(port);
    REQUIRE(waitForServer(*client, ctx.httpServer, runner));

    REQUIRE(client->Post("/feeds", feedBody(), "application/json")->status == 201);
    REQUIRE(client->Post("/scenes", sceneWithSurfaceBody("scene-1", "1"), "application/json")->status == 201);
    REQUIRE(client->Post("/cues", cueBody("cue-guard", "scene-1", "s1"), "application/json")->status == 201);
    REQUIRE(client->Post("/projects", projectBody("project-guard", {"cue-guard"}), "application/json")->status == 201);

    auto deleteRes = client->Delete("/cues/cue-guard");
    REQUIRE(deleteRes != nullptr);
    REQUIRE(deleteRes->status == 400);

    std::filesystem::remove(dbPath);
}

}  // namespace projection::server
