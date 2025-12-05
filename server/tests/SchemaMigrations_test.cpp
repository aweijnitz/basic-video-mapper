#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "db/SchemaMigrations.h"

using projection::server::db::SchemaMigrations;
using projection::server::db::SqliteConnection;

namespace {
std::filesystem::path makeTempDbPath(const std::string& name) {
    auto path = std::filesystem::temp_directory_path() / name;
    if (std::filesystem::exists(path)) {
        std::filesystem::remove(path);
    }
    return path;
}
}  // namespace

TEST_CASE("SchemaMigrations creates schema and records version", "[db][sqlite]") {
    auto dbPath = makeTempDbPath("projection_mapper_migrations.db");
    SqliteConnection connection(dbPath.string());
    connection.open();

    SchemaMigrations migrations;
    migrations.apply(connection);

    auto schemaRows = connection.query(
        "SELECT name FROM sqlite_master WHERE type='table' AND name IN ('scenes','feeds','surfaces','cues')");
    REQUIRE(schemaRows.size() == 4);

    auto versionRows = connection.query("SELECT version FROM schema_version");
    REQUIRE(versionRows.size() == 1);
    REQUIRE(versionRows.front().at("version") == "1");

    connection.close();
    std::filesystem::remove(dbPath);
}

TEST_CASE("SchemaMigrations is idempotent", "[db][sqlite]") {
    auto dbPath = makeTempDbPath("projection_mapper_migrations_idempotent.db");
    SqliteConnection connection(dbPath.string());
    connection.open();

    SchemaMigrations migrations;
    migrations.apply(connection);
    migrations.apply(connection);

    auto versionRows = connection.query("SELECT COUNT(*) AS count FROM schema_version");
    REQUIRE(versionRows.size() == 1);
    REQUIRE(versionRows.front().at("count") == "1");

    connection.close();
    std::filesystem::remove(dbPath);
}
