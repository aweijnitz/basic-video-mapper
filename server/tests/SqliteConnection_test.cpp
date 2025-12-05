#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "db/SqliteConnection.h"

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

TEST_CASE("SqliteConnection opens and executes statements", "[db][sqlite]") {
    auto dbPath = makeTempDbPath("projection_mapper_sqlite_connection.db");
    SqliteConnection connection(dbPath.string());

    connection.open();
    REQUIRE(connection.handle() != nullptr);

    connection.execute("CREATE TABLE entries(id INTEGER PRIMARY KEY, name TEXT NOT NULL)");
    connection.execute("INSERT INTO entries(name) VALUES (?)", {"alpha"});
    connection.execute("INSERT INTO entries(name) VALUES (?)", {"beta"});

    auto rows = connection.query("SELECT name FROM entries WHERE name = ?", {"alpha"});
    REQUIRE(rows.size() == 1);
    REQUIRE(rows.front().at("name") == "alpha");

    connection.close();
    std::filesystem::remove(dbPath);
}
