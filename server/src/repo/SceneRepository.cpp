#include "repo/SceneRepository.h"

#include <sqlite3.h>

#include <stdexcept>
#include <string>

namespace projection::server::repo {

namespace {
long long parseId(const core::SceneId& id) {
    if (id.value.empty()) {
        return -1;
    }
    try {
        return std::stoll(id.value);
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Invalid scene id: ") + e.what());
    }
}
}

SceneRepository::SceneRepository(db::SqliteConnection& connection) : connection_(connection) {}

core::Scene SceneRepository::createScene(const core::Scene& scene) {
    sqlite3* handle = connection_.getHandle();
    if (handle == nullptr) {
        throw std::runtime_error("SQLite connection is not open");
    }

    const bool hasId = !scene.getId().value.empty();
    std::string sql;
    if (hasId) {
        sql = "INSERT INTO scenes(id, name, description) VALUES(?, ?, ?);";
    } else {
        sql = "INSERT INTO scenes(name, description) VALUES(?, ?);";
    }

    sqlite3_stmt* stmt = nullptr;
    int result = sqlite3_prepare_v2(handle, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare scene insert statement: " + std::string(sqlite3_errmsg(handle)));
    }

    int bindIndex = 1;
    if (hasId) {
        long long idValue = parseId(scene.getId());
        result = sqlite3_bind_int64(stmt, bindIndex++, idValue);
        if (result != SQLITE_OK) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("Failed to bind scene id: " + std::string(sqlite3_errmsg(handle)));
        }
    }

    result = sqlite3_bind_text(stmt, bindIndex++, scene.getName().c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind scene name: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_bind_text(stmt, bindIndex, scene.getDescription().c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind scene description: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_step(stmt);
    if (result != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to insert scene: " + std::string(sqlite3_errmsg(handle)));
    }

    sqlite3_finalize(stmt);

    core::Scene created = scene;
    if (!hasId) {
        long long newId = sqlite3_last_insert_rowid(handle);
        created.setId(core::SceneId(std::to_string(newId)));
    }

    // Surfaces are not persisted for this milestone.
    created.setSurfaces({});

    return created;
}

std::vector<core::Scene> SceneRepository::listScenes() {
    sqlite3* handle = connection_.getHandle();
    if (handle == nullptr) {
        throw std::runtime_error("SQLite connection is not open");
    }

    const char* sql = "SELECT id, name, description FROM scenes ORDER BY id;";

    sqlite3_stmt* stmt = nullptr;
    int result = sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare scene select statement: " + std::string(sqlite3_errmsg(handle)));
    }

    std::vector<core::Scene> scenes;
    while ((result = sqlite3_step(stmt)) == SQLITE_ROW) {
        long long idValue = sqlite3_column_int64(stmt, 0);
        const unsigned char* nameText = sqlite3_column_text(stmt, 1);
        const unsigned char* descText = sqlite3_column_text(stmt, 2);

        std::string idString = std::to_string(idValue);
        std::string name = nameText ? reinterpret_cast<const char*>(nameText) : "";
        std::string description = descText ? reinterpret_cast<const char*>(descText) : "";

        scenes.emplace_back(core::Scene(core::SceneId(idString), name, description, {}));
    }

    if (result != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to read scenes: " + std::string(sqlite3_errmsg(handle)));
    }

    sqlite3_finalize(stmt);
    return scenes;
}

std::optional<core::Scene> SceneRepository::findSceneById(const core::SceneId& sceneId) {
    sqlite3* handle = connection_.getHandle();
    if (handle == nullptr) {
        throw std::runtime_error("SQLite connection is not open");
    }

    const char* sql = "SELECT id, name, description FROM scenes WHERE id = ? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    int result = sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare scene select statement: " + std::string(sqlite3_errmsg(handle)));
    }

    long long idValue = parseId(sceneId);
    result = sqlite3_bind_int64(stmt, 1, idValue);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind scene id: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_step(stmt);
    if (result == SQLITE_ROW) {
        const unsigned char* nameText = sqlite3_column_text(stmt, 1);
        const unsigned char* descText = sqlite3_column_text(stmt, 2);

        std::string name = nameText ? reinterpret_cast<const char*>(nameText) : "";
        std::string description = descText ? reinterpret_cast<const char*>(descText) : "";
        sqlite3_finalize(stmt);
        return core::Scene(sceneId, name, description, {});
    }

    if (result != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to read scene: " + std::string(sqlite3_errmsg(handle)));
    }

    sqlite3_finalize(stmt);
    return std::nullopt;
}

bool SceneRepository::sceneExists(const core::SceneId& sceneId) { return findSceneById(sceneId).has_value(); }

}  // namespace projection::server::repo
