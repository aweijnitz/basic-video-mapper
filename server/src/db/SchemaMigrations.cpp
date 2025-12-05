#include "db/SchemaMigrations.h"

#include "db/SqliteConnection.h"

#include <sqlite3.h>

#include <stdexcept>
#include <string>

namespace projection::server::db {

namespace {
const char* kCreateSchemaVersion = R"SQL(
CREATE TABLE IF NOT EXISTS schema_version (
    version INTEGER NOT NULL
);
)SQL";

const char* kCreateFeedsTable = R"SQL(
CREATE TABLE IF NOT EXISTS feeds (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    type TEXT NOT NULL,
    config_json TEXT NOT NULL
);
)SQL";

const char* kCreateScenesTable = R"SQL(
CREATE TABLE IF NOT EXISTS scenes (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    description TEXT
);
)SQL";

void ensureSchemaVersionTable(sqlite3* handle) {
    char* errorMessage = nullptr;
    int result = sqlite3_exec(handle, kCreateSchemaVersion, nullptr, nullptr, &errorMessage);
    if (result != SQLITE_OK) {
        std::string error = errorMessage ? errorMessage : "Unknown error";
        sqlite3_free(errorMessage);
        throw std::runtime_error("Failed to create schema_version table: " + error);
    }
}

void createTables(sqlite3* handle) {
    char* errorMessage = nullptr;
    int result = sqlite3_exec(handle, kCreateFeedsTable, nullptr, nullptr, &errorMessage);
    if (result != SQLITE_OK) {
        std::string error = errorMessage ? errorMessage : "Unknown error";
        sqlite3_free(errorMessage);
        throw std::runtime_error("Failed to create feeds table: " + error);
    }

    result = sqlite3_exec(handle, kCreateScenesTable, nullptr, nullptr, &errorMessage);
    if (result != SQLITE_OK) {
        std::string error = errorMessage ? errorMessage : "Unknown error";
        sqlite3_free(errorMessage);
        throw std::runtime_error("Failed to create scenes table: " + error);
    }
}
}  // namespace

void SchemaMigrations::applyMigrations(SqliteConnection& connection) {
    sqlite3* handle = connection.getHandle();
    if (handle == nullptr) {
        throw std::runtime_error("SQLite connection is not open");
    }

    ensureSchemaVersionTable(handle);
    createTables(handle);
}

}  // namespace projection::server::db
