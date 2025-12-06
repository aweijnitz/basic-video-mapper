#include "db/SchemaMigrations.h"
#include "db/SqliteConnection.h"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <sqlite3.h>
#include <string>

using projection::server::db::SchemaMigrations;
using projection::server::db::SqliteConnection;

namespace {
std::filesystem::path tempDb(const std::string& name) {
    return std::filesystem::temp_directory_path() / name;
}
}

TEST_CASE("SchemaMigrations creates surfaces table", "[db][migrations][surfaces]") {
    auto dbPath = tempDb("projection_mapper_surfaces.sqlite");
    std::filesystem::remove(dbPath);

    SqliteConnection connection;
    connection.open(dbPath.string());
    SchemaMigrations::applyMigrations(connection);

    sqlite3* handle = connection.getHandle();
    REQUIRE(handle != nullptr);

    char* errorMessage = nullptr;
    int result = sqlite3_exec(handle,
                              "INSERT INTO surfaces(id, scene_id, name, feed_id, z_order, opacity, brightness, blend_mode, "
                              "vertices_json) VALUES('s1', '1', 'Surf', 'f1', 0, 1.0, 1.0, 'Normal', '[]');",
                              nullptr, nullptr, &errorMessage);
    std::string errorStr = errorMessage ? errorMessage : "";
    sqlite3_free(errorMessage);

    REQUIRE(result == SQLITE_OK);
}
