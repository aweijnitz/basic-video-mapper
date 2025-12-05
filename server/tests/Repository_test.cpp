#include "db/SchemaMigrations.h"
#include "db/SqliteConnection.h"
#include "projection/core/Feed.h"
#include "projection/core/Scene.h"
#include "projection/core/Surface.h"
#include "repo/FeedRepository.h"
#include "repo/SceneRepository.h"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

using projection::core::Feed;
using projection::core::FeedType;
using projection::core::Scene;
using projection::core::makeFeedId;
using projection::core::makeSceneId;
using projection::server::db::SchemaMigrations;
using projection::server::db::SqliteConnection;
using projection::server::repo::FeedRepository;
using projection::server::repo::SceneRepository;

namespace {
std::filesystem::path createTempDbPath(const std::string& name) {
    auto tempDir = std::filesystem::temp_directory_path();
    return tempDir / name;
}

void setupTestDb(SqliteConnection& connection, const std::string& filename) {
    auto dbPath = createTempDbPath(filename);
    std::filesystem::remove(dbPath);
    connection.open(dbPath.string());
    SchemaMigrations::applyMigrations(connection);
}

bool expectRuntimeError(const std::function<void()>& fn) {
    try {
        fn();
    } catch (const std::runtime_error&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}
}

TEST_CASE("FeedRepository creates and lists feeds", "[repo][feed]") {
    SqliteConnection connection;
    setupTestDb(connection, "feed_repo.sqlite");

    FeedRepository repo(connection);

    Feed feedWithoutId(makeFeedId(""), "Test Feed", FeedType::VideoFile, R"({\"path\": \"video.mp4\"})");
    Feed created = repo.createFeed(feedWithoutId);
    REQUIRE(!created.getId().value.empty());
    REQUIRE(created.getName() == "Test Feed");

    Feed explicitIdFeed(makeFeedId("42"), "Second", FeedType::Camera, "{}");
    Feed createdWithId = repo.createFeed(explicitIdFeed);
    REQUIRE(createdWithId.getId().value == "42");

    auto feeds = repo.listFeeds();
    REQUIRE(feeds.size() == 2);
    REQUIRE(feeds[0].getName() == "Test Feed");
    REQUIRE(feeds[1].getId().value == "42");
    REQUIRE(feeds[1].getType() == FeedType::Camera);
}

TEST_CASE("SceneRepository creates and lists scenes", "[repo][scene]") {
    SqliteConnection connection;
    setupTestDb(connection, "scene_repo.sqlite");

    SceneRepository repo(connection);

    Scene sceneNoId(makeSceneId(""), "My Scene", "First scene", {});
    Scene created = repo.createScene(sceneNoId);
    REQUIRE(!created.getId().value.empty());

    Scene sceneWithId(makeSceneId("7"), "Another", "More", {});
    Scene createdWithId = repo.createScene(sceneWithId);
    REQUIRE(createdWithId.getId().value == "7");

    auto scenes = repo.listScenes();
    REQUIRE(scenes.size() == 2);
    REQUIRE(scenes[0].getName() == "My Scene");
    REQUIRE(scenes[1].getDescription() == "More");
    REQUIRE(scenes[1].getSurfaces().empty());
}

TEST_CASE("FeedRepository rejects invalid ids", "[repo][feed][error]") {
    SqliteConnection connection;
    setupTestDb(connection, "feed_repo_error.sqlite");

    FeedRepository repo(connection);
    Feed invalidId(makeFeedId("abc"), "Bad", FeedType::Generated, "{}");

    REQUIRE(expectRuntimeError([&]() { repo.createFeed(invalidId); }));
}

TEST_CASE("SceneRepository prevents duplicate numeric ids", "[repo][scene][error]") {
    SqliteConnection connection;
    setupTestDb(connection, "scene_repo_error.sqlite");

    SceneRepository repo(connection);
    Scene sceneA(makeSceneId("5"), "Primary", "desc", {});
    Scene sceneB(makeSceneId("5"), "Duplicate", "desc", {});

    repo.createScene(sceneA);
    REQUIRE(expectRuntimeError([&]() { repo.createScene(sceneB); }));
}
