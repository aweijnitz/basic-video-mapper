#include "repo/ProjectRepository.h"

#include <nlohmann/json.hpp>
#include <sqlite3.h>

#include <stdexcept>
#include <string>

#include "projection/core/Serialization.h"

namespace projection::server::repo {

ProjectRepository::ProjectRepository(db::SqliteConnection& connection) : connection_(connection) {}

void ProjectRepository::persistCueOrder(const core::ProjectId& projectId, const std::vector<core::CueId>& cueOrder,
                                        sqlite3* handle) {
    const char* deleteSql = "DELETE FROM project_cues WHERE project_id=?;";
    sqlite3_stmt* deleteStmt = nullptr;
    int rc = sqlite3_prepare_v2(handle, deleteSql, -1, &deleteStmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare project_cues delete: " + std::string(sqlite3_errmsg(handle)));
    }
    rc = sqlite3_bind_text(deleteStmt, 1, projectId.value.c_str(), -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(deleteStmt);
        throw std::runtime_error("Failed to bind project id for cue deletion: " + std::string(sqlite3_errmsg(handle)));
    }
    rc = sqlite3_step(deleteStmt);
    if (rc != SQLITE_DONE) {
        sqlite3_finalize(deleteStmt);
        throw std::runtime_error("Failed to clear project cue order: " + std::string(sqlite3_errmsg(handle)));
    }
    sqlite3_finalize(deleteStmt);

    const char* insertSql = "INSERT INTO project_cues(project_id, cue_id, position) VALUES(?, ?, ?);";
    sqlite3_stmt* insertStmt = nullptr;
    rc = sqlite3_prepare_v2(handle, insertSql, -1, &insertStmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare project_cues insert: " + std::string(sqlite3_errmsg(handle)));
    }
    int position = 0;
    for (const auto& cueId : cueOrder) {
        rc = sqlite3_bind_text(insertStmt, 1, projectId.value.c_str(), -1, SQLITE_TRANSIENT);
        rc |= sqlite3_bind_text(insertStmt, 2, cueId.value.c_str(), -1, SQLITE_TRANSIENT);
        rc |= sqlite3_bind_int(insertStmt, 3, position++);
        if (rc != SQLITE_OK) {
            sqlite3_finalize(insertStmt);
            throw std::runtime_error("Failed to bind project_cues insert fields: " + std::string(sqlite3_errmsg(handle)));
        }
        rc = sqlite3_step(insertStmt);
        if (rc != SQLITE_DONE) {
            sqlite3_finalize(insertStmt);
            throw std::runtime_error("Failed to insert project cue: " + std::string(sqlite3_errmsg(handle)));
        }
        sqlite3_reset(insertStmt);
        sqlite3_clear_bindings(insertStmt);
    }
    sqlite3_finalize(insertStmt);
}

core::Project ProjectRepository::createProject(const core::Project& project) {
    sqlite3* handle = connection_.getHandle();
    if (!handle) {
        throw std::runtime_error("SQLite connection is not open");
    }
    if (project.getId().value.empty()) {
        throw std::runtime_error("Project id must not be empty");
    }

    const auto settingsJson = nlohmann::json(project.getSettings()).dump();
    const char* sql = "INSERT INTO projects(id, name, description, settings_json) VALUES(?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare project insert: " + std::string(sqlite3_errmsg(handle)));
    }
    rc = sqlite3_bind_text(stmt, 1, project.getId().value.c_str(), -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_text(stmt, 2, project.getName().c_str(), -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_text(stmt, 3, project.getDescription().c_str(), -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_text(stmt, 4, settingsJson.c_str(), -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind project fields: " + std::string(sqlite3_errmsg(handle)));
    }
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to insert project: " + std::string(sqlite3_errmsg(handle)));
    }
    sqlite3_finalize(stmt);

    persistCueOrder(project.getId(), project.getCueOrder(), handle);
    return project;
}

std::vector<core::Project> ProjectRepository::listProjects() {
    sqlite3* handle = connection_.getHandle();
    if (!handle) {
        throw std::runtime_error("SQLite connection is not open");
    }

    const char* sql = "SELECT id, name, description, settings_json FROM projects ORDER BY id;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare project select: " + std::string(sqlite3_errmsg(handle)));
    }

    std::vector<core::Project> projects;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const unsigned char* idText = sqlite3_column_text(stmt, 0);
        const unsigned char* nameText = sqlite3_column_text(stmt, 1);
        const unsigned char* descText = sqlite3_column_text(stmt, 2);
        const unsigned char* settingsText = sqlite3_column_text(stmt, 3);

        std::string id = idText ? reinterpret_cast<const char*>(idText) : "";
        std::string name = nameText ? reinterpret_cast<const char*>(nameText) : "";
        std::string description = descText ? reinterpret_cast<const char*>(descText) : "";
        auto settingsJson = nlohmann::json::parse(settingsText ? reinterpret_cast<const char*>(settingsText) : "{}");
        core::ProjectSettings settings = settingsJson.get<core::ProjectSettings>();
        projects.emplace_back(core::Project(core::ProjectId{id}, name, description, {}, settings));
    }
    if (rc != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to read projects: " + std::string(sqlite3_errmsg(handle)));
    }
    sqlite3_finalize(stmt);

    const char* cuesSql = "SELECT cue_id FROM project_cues WHERE project_id=? ORDER BY position ASC;";
    sqlite3_stmt* cuesStmt = nullptr;
    rc = sqlite3_prepare_v2(handle, cuesSql, -1, &cuesStmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare project_cues select: " + std::string(sqlite3_errmsg(handle)));
    }

    for (auto& project : projects) {
        rc = sqlite3_bind_text(cuesStmt, 1, project.getId().value.c_str(), -1, SQLITE_TRANSIENT);
        if (rc != SQLITE_OK) {
            sqlite3_finalize(cuesStmt);
            throw std::runtime_error("Failed to bind project id for cue select: " + std::string(sqlite3_errmsg(handle)));
        }
        std::vector<core::CueId> cueOrder;
        while ((rc = sqlite3_step(cuesStmt)) == SQLITE_ROW) {
            const unsigned char* cueText = sqlite3_column_text(cuesStmt, 0);
            cueOrder.emplace_back(cueText ? reinterpret_cast<const char*>(cueText) : "");
        }
        if (rc != SQLITE_DONE) {
            sqlite3_finalize(cuesStmt);
            throw std::runtime_error("Failed to read project cues: " + std::string(sqlite3_errmsg(handle)));
        }
        project.setCueOrder(cueOrder);
        sqlite3_reset(cuesStmt);
        sqlite3_clear_bindings(cuesStmt);
    }
    sqlite3_finalize(cuesStmt);

    return projects;
}

std::optional<core::Project> ProjectRepository::findProjectById(const core::ProjectId& projectId) {
    sqlite3* handle = connection_.getHandle();
    if (!handle) {
        throw std::runtime_error("SQLite connection is not open");
    }

    const char* sql = "SELECT id, name, description, settings_json FROM projects WHERE id=? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare project select by id: " + std::string(sqlite3_errmsg(handle)));
    }
    rc = sqlite3_bind_text(stmt, 1, projectId.value.c_str(), -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind project id: " + std::string(sqlite3_errmsg(handle)));
    }
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const unsigned char* nameText = sqlite3_column_text(stmt, 1);
        const unsigned char* descText = sqlite3_column_text(stmt, 2);
        const unsigned char* settingsText = sqlite3_column_text(stmt, 3);

        std::string name = nameText ? reinterpret_cast<const char*>(nameText) : "";
        std::string description = descText ? reinterpret_cast<const char*>(descText) : "";
        auto settingsJson = nlohmann::json::parse(settingsText ? reinterpret_cast<const char*>(settingsText) : "{}");
        core::ProjectSettings settings = settingsJson.get<core::ProjectSettings>();
        sqlite3_finalize(stmt);

        const char* cuesSql = "SELECT cue_id FROM project_cues WHERE project_id=? ORDER BY position ASC;";
        sqlite3_stmt* cuesStmt = nullptr;
        rc = sqlite3_prepare_v2(handle, cuesSql, -1, &cuesStmt, nullptr);
        if (rc != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare project cue select: " + std::string(sqlite3_errmsg(handle)));
        }
        rc = sqlite3_bind_text(cuesStmt, 1, projectId.value.c_str(), -1, SQLITE_TRANSIENT);
        if (rc != SQLITE_OK) {
            sqlite3_finalize(cuesStmt);
            throw std::runtime_error("Failed to bind project id for cue select: " + std::string(sqlite3_errmsg(handle)));
        }
        std::vector<core::CueId> cueOrder;
        while ((rc = sqlite3_step(cuesStmt)) == SQLITE_ROW) {
            const unsigned char* cueText = sqlite3_column_text(cuesStmt, 0);
            cueOrder.emplace_back(cueText ? reinterpret_cast<const char*>(cueText) : "");
        }
        if (rc != SQLITE_DONE) {
            sqlite3_finalize(cuesStmt);
            throw std::runtime_error("Failed to read project cues: " + std::string(sqlite3_errmsg(handle)));
        }
        sqlite3_finalize(cuesStmt);
        return core::Project(projectId, name, description, cueOrder, settings);
    }
    sqlite3_finalize(stmt);
    return std::nullopt;
}

core::Project ProjectRepository::updateProject(const core::Project& project) {
    sqlite3* handle = connection_.getHandle();
    if (!handle) {
        throw std::runtime_error("SQLite connection is not open");
    }
    if (project.getId().value.empty()) {
        throw std::runtime_error("Project id must not be empty for update");
    }

    const auto settingsJson = nlohmann::json(project.getSettings()).dump();
    const char* sql = "UPDATE projects SET name=?, description=?, settings_json=? WHERE id=?;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare project update: " + std::string(sqlite3_errmsg(handle)));
    }
    rc = sqlite3_bind_text(stmt, 1, project.getName().c_str(), -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_text(stmt, 2, project.getDescription().c_str(), -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_text(stmt, 3, settingsJson.c_str(), -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_text(stmt, 4, project.getId().value.c_str(), -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind project update fields: " + std::string(sqlite3_errmsg(handle)));
    }
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to update project: " + std::string(sqlite3_errmsg(handle)));
    }
    sqlite3_finalize(stmt);

    persistCueOrder(project.getId(), project.getCueOrder(), handle);
    return project;
}

void ProjectRepository::deleteProject(const core::ProjectId& projectId) {
    sqlite3* handle = connection_.getHandle();
    if (!handle) {
        throw std::runtime_error("SQLite connection is not open");
    }

    const char* sql = "DELETE FROM projects WHERE id=?;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare project delete: " + std::string(sqlite3_errmsg(handle)));
    }
    rc = sqlite3_bind_text(stmt, 1, projectId.value.c_str(), -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind project id for delete: " + std::string(sqlite3_errmsg(handle)));
    }
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to delete project: " + std::string(sqlite3_errmsg(handle)));
    }
    sqlite3_finalize(stmt);

    // Clean cue order explicitly to be robust if foreign keys are disabled.
    const char* deleteCuesSql = "DELETE FROM project_cues WHERE project_id=?;";
    sqlite3_stmt* cuesStmt = nullptr;
    rc = sqlite3_prepare_v2(handle, deleteCuesSql, -1, &cuesStmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare project cue delete: " + std::string(sqlite3_errmsg(handle)));
    }
    rc = sqlite3_bind_text(cuesStmt, 1, projectId.value.c_str(), -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(cuesStmt);
        throw std::runtime_error("Failed to bind project id for project cue delete: " + std::string(sqlite3_errmsg(handle)));
    }
    rc = sqlite3_step(cuesStmt);
    if (rc != SQLITE_DONE) {
        sqlite3_finalize(cuesStmt);
        throw std::runtime_error("Failed to delete project cues: " + std::string(sqlite3_errmsg(handle)));
    }
    sqlite3_finalize(cuesStmt);
}

bool ProjectRepository::projectExists(const core::ProjectId& projectId) { return findProjectById(projectId).has_value(); }

}  // namespace projection::server::repo
