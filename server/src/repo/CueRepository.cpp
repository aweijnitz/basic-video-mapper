#include "repo/CueRepository.h"

#include <nlohmann/json.hpp>
#include <sqlite3.h>

#include <stdexcept>
#include <string>

namespace projection::server::repo {
namespace {
nlohmann::json serializeSurfaceValues(const std::map<core::SurfaceId, float>& values) {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& [id, value] : values) {
        arr.push_back({{"surfaceId", id.value}, {"value", value}});
    }
    return arr;
}
}  // namespace

CueRepository::CueRepository(db::SqliteConnection& connection) : connection_(connection) {}

core::Cue CueRepository::createCue(const core::Cue& cue) {
    sqlite3* handle = connection_.getHandle();
    if (!handle) {
        throw std::runtime_error("SQLite connection is not open");
    }
    if (cue.getId().value.empty()) {
        throw std::runtime_error("Cue id must not be empty");
    }

    const nlohmann::json opacitiesJson = serializeSurfaceValues(cue.getSurfaceOpacities());
    const nlohmann::json brightnessJson = serializeSurfaceValues(cue.getSurfaceBrightnesses());

    const char* sql = "INSERT INTO cues(id, name, scene_id, surface_opacities_json, surface_brightnesses_json) "
                      "VALUES(?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare cue insert: " + std::string(sqlite3_errmsg(handle)));
    }

    rc = sqlite3_bind_text(stmt, 1, cue.getId().value.c_str(), -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_text(stmt, 2, cue.getName().c_str(), -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_text(stmt, 3, cue.getSceneId().value.c_str(), -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_text(stmt, 4, opacitiesJson.dump().c_str(), -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_text(stmt, 5, brightnessJson.dump().c_str(), -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind cue fields: " + std::string(sqlite3_errmsg(handle)));
    }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to insert cue: " + std::string(sqlite3_errmsg(handle)));
    }
    sqlite3_finalize(stmt);
    return cue;
}

std::vector<core::Cue> CueRepository::listCues() {
    sqlite3* handle = connection_.getHandle();
    if (!handle) {
        throw std::runtime_error("SQLite connection is not open");
    }
    const char* sql = "SELECT id, name, scene_id, surface_opacities_json, surface_brightnesses_json FROM cues;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare cue select: " + std::string(sqlite3_errmsg(handle)));
    }
    std::vector<core::Cue> cues;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const unsigned char* idText = sqlite3_column_text(stmt, 0);
        const unsigned char* nameText = sqlite3_column_text(stmt, 1);
        const unsigned char* sceneText = sqlite3_column_text(stmt, 2);
        const unsigned char* opacitiesText = sqlite3_column_text(stmt, 3);
        const unsigned char* brightnessText = sqlite3_column_text(stmt, 4);

        core::Cue cue(core::CueId{idText ? reinterpret_cast<const char*>(idText) : ""},
                      nameText ? reinterpret_cast<const char*>(nameText) : "",
                      core::SceneId{sceneText ? reinterpret_cast<const char*>(sceneText) : ""});

        auto opacitiesJson = nlohmann::json::parse(opacitiesText ? reinterpret_cast<const char*>(opacitiesText) : "[]");
        auto brightnessJson =
            nlohmann::json::parse(brightnessText ? reinterpret_cast<const char*>(brightnessText) : "[]");
        cue.getSurfaceOpacities().clear();
        cue.getSurfaceBrightnesses().clear();
        for (const auto& entry : opacitiesJson) {
            cue.getSurfaceOpacities().emplace(core::SurfaceId{entry.at("surfaceId").get<std::string>()},
                                              entry.at("value").get<float>());
        }
        for (const auto& entry : brightnessJson) {
            cue.getSurfaceBrightnesses().emplace(core::SurfaceId{entry.at("surfaceId").get<std::string>()},
                                                 entry.at("value").get<float>());
        }
        cues.push_back(std::move(cue));
    }
    if (rc != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to read cues: " + std::string(sqlite3_errmsg(handle)));
    }
    sqlite3_finalize(stmt);
    return cues;
}

std::optional<core::Cue> CueRepository::findCueById(const core::CueId& id) {
    sqlite3* handle = connection_.getHandle();
    if (!handle) {
        throw std::runtime_error("SQLite connection is not open");
    }
    const char* sql = "SELECT id, name, scene_id, surface_opacities_json, surface_brightnesses_json FROM cues WHERE id=?;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare cue select: " + std::string(sqlite3_errmsg(handle)));
    }
    rc = sqlite3_bind_text(stmt, 1, id.value.c_str(), -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind cue id: " + std::string(sqlite3_errmsg(handle)));
    }
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const unsigned char* idText = sqlite3_column_text(stmt, 0);
        const unsigned char* nameText = sqlite3_column_text(stmt, 1);
        const unsigned char* sceneText = sqlite3_column_text(stmt, 2);
        const unsigned char* opacitiesText = sqlite3_column_text(stmt, 3);
        const unsigned char* brightnessText = sqlite3_column_text(stmt, 4);

        core::Cue cue(core::CueId{idText ? reinterpret_cast<const char*>(idText) : ""},
                      nameText ? reinterpret_cast<const char*>(nameText) : "",
                      core::SceneId{sceneText ? reinterpret_cast<const char*>(sceneText) : ""});
        cue.getSurfaceOpacities().clear();
        cue.getSurfaceBrightnesses().clear();
        auto opacitiesJson = nlohmann::json::parse(opacitiesText ? reinterpret_cast<const char*>(opacitiesText) : "[]");
        auto brightnessJson =
            nlohmann::json::parse(brightnessText ? reinterpret_cast<const char*>(brightnessText) : "[]");
        for (const auto& entry : opacitiesJson) {
            cue.getSurfaceOpacities().emplace(core::SurfaceId{entry.at("surfaceId").get<std::string>()},
                                              entry.at("value").get<float>());
        }
        for (const auto& entry : brightnessJson) {
            cue.getSurfaceBrightnesses().emplace(core::SurfaceId{entry.at("surfaceId").get<std::string>()},
                                                 entry.at("value").get<float>());
        }
        sqlite3_finalize(stmt);
        return cue;
    }
    sqlite3_finalize(stmt);
    return std::nullopt;
}

core::Cue CueRepository::updateCue(const core::Cue& cue) {
    sqlite3* handle = connection_.getHandle();
    if (!handle) {
        throw std::runtime_error("SQLite connection is not open");
    }
    const char* sql = "UPDATE cues SET name=?, scene_id=?, surface_opacities_json=?, surface_brightnesses_json=? "
                      "WHERE id=?;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare cue update: " + std::string(sqlite3_errmsg(handle)));
    }

    const nlohmann::json opacitiesJson = serializeSurfaceValues(cue.getSurfaceOpacities());
    const nlohmann::json brightnessJson = serializeSurfaceValues(cue.getSurfaceBrightnesses());

    rc = sqlite3_bind_text(stmt, 1, cue.getName().c_str(), -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_text(stmt, 2, cue.getSceneId().value.c_str(), -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_text(stmt, 3, opacitiesJson.dump().c_str(), -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_text(stmt, 4, brightnessJson.dump().c_str(), -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_text(stmt, 5, cue.getId().value.c_str(), -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind cue update fields: " + std::string(sqlite3_errmsg(handle)));
    }
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to update cue: " + std::string(sqlite3_errmsg(handle)));
    }
    sqlite3_finalize(stmt);
    return cue;
}

void CueRepository::deleteCue(const core::CueId& id) {
    sqlite3* handle = connection_.getHandle();
    if (!handle) {
        throw std::runtime_error("SQLite connection is not open");
    }
    const char* sql = "DELETE FROM cues WHERE id=?;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare cue delete: " + std::string(sqlite3_errmsg(handle)));
    }
    rc = sqlite3_bind_text(stmt, 1, id.value.c_str(), -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind cue id: " + std::string(sqlite3_errmsg(handle)));
    }
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to delete cue: " + std::string(sqlite3_errmsg(handle)));
    }
    sqlite3_finalize(stmt);
}

}  // namespace projection::server::repo
