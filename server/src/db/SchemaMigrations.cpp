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
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    type TEXT NOT NULL,
    config_json TEXT NOT NULL
);
)SQL";

const char* kCreateScenesTable = R"SQL(
CREATE TABLE IF NOT EXISTS scenes (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    description TEXT
);
)SQL";

const char* kCreateSurfacesTable = R"SQL(
CREATE TABLE IF NOT EXISTS surfaces (
    id TEXT PRIMARY KEY,
    scene_id TEXT NOT NULL,
    name TEXT NOT NULL,
    feed_id TEXT NOT NULL,
    z_order INTEGER NOT NULL,
    opacity REAL NOT NULL,
    brightness REAL NOT NULL,
    blend_mode TEXT NOT NULL,
    vertices_json TEXT NOT NULL,
    FOREIGN KEY(scene_id) REFERENCES scenes(id)
);
)SQL";

const char* kCreateCuesTable = R"SQL(
CREATE TABLE IF NOT EXISTS cues (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    scene_id TEXT NOT NULL,
    surface_opacities_json TEXT NOT NULL,
    surface_brightnesses_json TEXT NOT NULL,
    FOREIGN KEY(scene_id) REFERENCES scenes(id)
);
)SQL";

const char* kCreateProjectsTable = R"SQL(
CREATE TABLE IF NOT EXISTS projects (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    description TEXT,
    settings_json TEXT NOT NULL
);
)SQL";

const char* kCreateProjectCuesTable = R"SQL(
CREATE TABLE IF NOT EXISTS project_cues (
    project_id TEXT NOT NULL,
    cue_id TEXT NOT NULL,
    position INTEGER NOT NULL,
    PRIMARY KEY(project_id, position),
    FOREIGN KEY(project_id) REFERENCES projects(id) ON DELETE CASCADE,
    FOREIGN KEY(cue_id) REFERENCES cues(id)
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

    result = sqlite3_exec(handle, kCreateSurfacesTable, nullptr, nullptr, &errorMessage);
    if (result != SQLITE_OK) {
        std::string error = errorMessage ? errorMessage : "Unknown error";
        sqlite3_free(errorMessage);
        throw std::runtime_error("Failed to create surfaces table: " + error);
    }

    result = sqlite3_exec(handle, kCreateCuesTable, nullptr, nullptr, &errorMessage);
    if (result != SQLITE_OK) {
        std::string error = errorMessage ? errorMessage : "Unknown error";
        sqlite3_free(errorMessage);
        throw std::runtime_error("Failed to create cues table: " + error);
    }

    result = sqlite3_exec(handle, kCreateProjectsTable, nullptr, nullptr, &errorMessage);
    if (result != SQLITE_OK) {
        std::string error = errorMessage ? errorMessage : "Unknown error";
        sqlite3_free(errorMessage);
        throw std::runtime_error("Failed to create projects table: " + error);
    }

    result = sqlite3_exec(handle, kCreateProjectCuesTable, nullptr, nullptr, &errorMessage);
    if (result != SQLITE_OK) {
        std::string error = errorMessage ? errorMessage : "Unknown error";
        sqlite3_free(errorMessage);
        throw std::runtime_error("Failed to create project_cues table: " + error);
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
