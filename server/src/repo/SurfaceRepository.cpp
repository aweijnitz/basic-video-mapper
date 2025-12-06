#include "repo/SurfaceRepository.h"

#include <sqlite3.h>

#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

#include "projection/core/Serialization.h"

namespace projection::server::repo {

SurfaceRepository::SurfaceRepository(db::SqliteConnection& connection) : connection_(connection) {}

core::Surface SurfaceRepository::createSurface(const core::Surface& surface, const core::SceneId& sceneId) {
    sqlite3* handle = connection_.getHandle();
    if (handle == nullptr) {
        throw std::runtime_error("SQLite connection is not open");
    }

    if (surface.getId().value.empty()) {
        throw std::runtime_error("Surface id must not be empty");
    }

    const char* sql =
        "INSERT INTO surfaces(id, scene_id, name, feed_id, z_order, opacity, brightness, blend_mode, vertices_json) "
        "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;
    int result = sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare surface insert statement: " + std::string(sqlite3_errmsg(handle)));
    }

    const nlohmann::json verticesJson = surface.getVertices();
    const std::string verticesJsonStr = verticesJson.dump();
    const std::string blendMode = core::toString(surface.getBlendMode());

    int bindIndex = 1;
    result = sqlite3_bind_text(stmt, bindIndex++, surface.getId().value.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind surface id: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_bind_text(stmt, bindIndex++, sceneId.value.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind scene id: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_bind_text(stmt, bindIndex++, surface.getName().c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind surface name: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_bind_text(stmt, bindIndex++, surface.getFeedId().value.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind surface feed id: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_bind_int(stmt, bindIndex++, surface.getZOrder());
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind surface z_order: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_bind_double(stmt, bindIndex++, surface.getOpacity());
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind surface opacity: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_bind_double(stmt, bindIndex++, surface.getBrightness());
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind surface brightness: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_bind_text(stmt, bindIndex++, blendMode.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind surface blend mode: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_bind_text(stmt, bindIndex, verticesJsonStr.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind surface vertices: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_step(stmt);
    if (result != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to insert surface: " + std::string(sqlite3_errmsg(handle)));
    }

    sqlite3_finalize(stmt);
    return surface;
}

std::vector<core::Surface> SurfaceRepository::listSurfacesForScene(const core::SceneId& sceneId) {
    sqlite3* handle = connection_.getHandle();
    if (handle == nullptr) {
        throw std::runtime_error("SQLite connection is not open");
    }

    const char* sql =
        "SELECT id, name, vertices_json, feed_id, opacity, brightness, blend_mode, z_order FROM surfaces WHERE scene_id = ? "
        "ORDER BY z_order, id;";

    sqlite3_stmt* stmt = nullptr;
    int result = sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare surface select statement: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_bind_text(stmt, 1, sceneId.value.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind scene id: " + std::string(sqlite3_errmsg(handle)));
    }

    std::vector<core::Surface> surfaces;
    while ((result = sqlite3_step(stmt)) == SQLITE_ROW) {
        const unsigned char* idText = sqlite3_column_text(stmt, 0);
        const unsigned char* nameText = sqlite3_column_text(stmt, 1);
        const unsigned char* verticesText = sqlite3_column_text(stmt, 2);
        const unsigned char* feedIdText = sqlite3_column_text(stmt, 3);
        double opacity = sqlite3_column_double(stmt, 4);
        double brightness = sqlite3_column_double(stmt, 5);
        const unsigned char* blendModeText = sqlite3_column_text(stmt, 6);
        int zOrder = sqlite3_column_int(stmt, 7);

        std::string id = idText ? reinterpret_cast<const char*>(idText) : "";
        std::string name = nameText ? reinterpret_cast<const char*>(nameText) : "";
        std::string verticesJson = verticesText ? reinterpret_cast<const char*>(verticesText) : "";
        std::string feedId = feedIdText ? reinterpret_cast<const char*>(feedIdText) : "";
        std::string blendModeStr = blendModeText ? reinterpret_cast<const char*>(blendModeText) : "";

        nlohmann::json verticesParsed = nlohmann::json::parse(verticesJson);
        std::vector<core::Vec2> vertices = verticesParsed.get<std::vector<core::Vec2>>();

        core::BlendMode blendMode;
        if (!core::fromString(blendModeStr, blendMode)) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("Failed to parse blend mode for surface: " + blendModeStr);
        }

        surfaces.emplace_back(core::Surface(core::SurfaceId{id}, name, vertices, core::FeedId{feedId},
                                            static_cast<float>(opacity), static_cast<float>(brightness), blendMode,
                                            zOrder));
    }

    if (result != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to read surfaces: " + std::string(sqlite3_errmsg(handle)));
    }

    sqlite3_finalize(stmt);
    return surfaces;
}

}  // namespace projection::server::repo
