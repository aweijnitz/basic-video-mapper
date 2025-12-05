#include "db/SchemaMigrations.h"

#include <stdexcept>
#include <string>

namespace projection::server::db {

SchemaMigrations::SchemaMigrations(int targetVersion) : targetVersion(targetVersion) {}

void SchemaMigrations::apply(SqliteConnection& connection) {
    connection.execute("PRAGMA foreign_keys = ON;");
    connection.execute("CREATE TABLE IF NOT EXISTS schema_version (version INTEGER NOT NULL, applied_at TEXT NOT NULL)");

    int currentVersion = readCurrentVersion(connection);
    if (currentVersion >= targetVersion) {
        return;
    }

    if (currentVersion < 1 && targetVersion >= 1) {
        applyInitialMigration(connection);
        connection.execute(
            "INSERT INTO schema_version(version, applied_at) VALUES (?, datetime('now'))", {std::to_string(1)});
    }
}

int SchemaMigrations::readCurrentVersion(SqliteConnection& connection) {
    auto rows = connection.query("SELECT MAX(version) AS version FROM schema_version");
    if (rows.empty()) {
        return 0;
    }

    const auto& versionValue = rows.front().at("version");
    if (versionValue.empty()) {
        return 0;
    }

    return std::stoi(versionValue);
}

void SchemaMigrations::applyInitialMigration(SqliteConnection& connection) {
    connection.execute(
        "CREATE TABLE IF NOT EXISTS scenes (id TEXT PRIMARY KEY, name TEXT NOT NULL, description TEXT DEFAULT '')");
    connection.execute(
        "CREATE TABLE IF NOT EXISTS feeds (id TEXT PRIMARY KEY, uri TEXT NOT NULL, type TEXT DEFAULT 'file')");
    connection.execute(
        "CREATE TABLE IF NOT EXISTS surfaces (id TEXT PRIMARY KEY, scene_id TEXT NOT NULL, name TEXT NOT NULL, "
        "FOREIGN KEY(scene_id) REFERENCES scenes(id) ON DELETE CASCADE)");
    connection.execute(
        "CREATE TABLE IF NOT EXISTS cues (id TEXT PRIMARY KEY, scene_id TEXT NOT NULL, name TEXT NOT NULL, "
        "FOREIGN KEY(scene_id) REFERENCES scenes(id) ON DELETE CASCADE)");
}

}  // namespace projection::server::db
