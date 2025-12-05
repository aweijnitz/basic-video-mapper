#include "db/SchemaMigrations.h"
#include "db/SqliteConnection.h"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <sqlite3.h>
#include <stdexcept>
#include <string>

using projection::server::db::SchemaMigrations;
using projection::server::db::SqliteConnection;

namespace {
std::filesystem::path createTempDbPath(const std::string& name) {
    auto tempDir = std::filesystem::temp_directory_path();
    return tempDir / name;
}
}

TEST_CASE("SqliteConnection opens and closes database", "[db][sqlite]") {
    auto dbPath = createTempDbPath("projection_mapper_test.sqlite");
    std::filesystem::remove(dbPath);

    SqliteConnection connection;
    bool threw = false;
    try {
        connection.open(dbPath.string());
    } catch (...) {
        threw = true;
    }

    REQUIRE(!threw);
    REQUIRE(connection.getHandle() != nullptr);
}

TEST_CASE("SchemaMigrations creates feeds and scenes tables", "[db][migrations]") {
    auto dbPath = createTempDbPath("projection_mapper_migrations.sqlite");
    std::filesystem::remove(dbPath);

    SqliteConnection connection;
    connection.open(dbPath.string());

    bool migrateThrew = false;
    try {
        SchemaMigrations::applyMigrations(connection);
    } catch (...) {
        migrateThrew = true;
    }
    REQUIRE(!migrateThrew);

    sqlite3* handle = connection.getHandle();
    REQUIRE(handle != nullptr);

    char* errorMessage = nullptr;
    int result = sqlite3_exec(handle, "INSERT INTO feeds(name, type, config_json) VALUES('feed1', 'video', '{}');", nullptr,
                              nullptr, &errorMessage);
    sqlite3_free(errorMessage);
    REQUIRE(result == SQLITE_OK);

    result = sqlite3_exec(handle, "INSERT INTO scenes(name, description) VALUES('scene1', 'desc');", nullptr, nullptr,
                          &errorMessage);
    sqlite3_free(errorMessage);
    REQUIRE(result == SQLITE_OK);

    bool sawFeed = false;
    auto feedCallback = [](void* data, int argc, char** argv, char** colNames) -> int {
        bool* found = static_cast<bool*>(data);
        if (argc > 0 && argv[0] != nullptr) {
            *found = true;
        }
        (void)colNames;
        return 0;
    };

    result = sqlite3_exec(handle, "SELECT name FROM feeds WHERE name='feed1';", feedCallback, &sawFeed, &errorMessage);
    sqlite3_free(errorMessage);
    REQUIRE(result == SQLITE_OK);
    REQUIRE(sawFeed);

    bool sawScene = false;
    auto sceneCallback = [](void* data, int argc, char** argv, char** colNames) -> int {
        bool* found = static_cast<bool*>(data);
        if (argc > 0 && argv[0] != nullptr) {
            *found = true;
        }
        (void)colNames;
        return 0;
    };

    result = sqlite3_exec(handle, "SELECT name FROM scenes WHERE name='scene1';", sceneCallback, &sawScene, &errorMessage);
    sqlite3_free(errorMessage);
    REQUIRE(result == SQLITE_OK);
    REQUIRE(sawScene);
}
