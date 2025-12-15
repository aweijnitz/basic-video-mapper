#pragma once

#include <optional>
#include <vector>

#include <sqlite3.h>

#include "db/SqliteConnection.h"
#include "projection/core/Project.h"

namespace projection::server::repo {

// Repository for managing projects and their ordered cue lists.
class ProjectRepository {
public:
    explicit ProjectRepository(db::SqliteConnection& connection);

    core::Project createProject(const core::Project& project);
    std::vector<core::Project> listProjects();
    std::optional<core::Project> findProjectById(const core::ProjectId& projectId);
    core::Project updateProject(const core::Project& project);
    void deleteProject(const core::ProjectId& projectId);

    bool projectExists(const core::ProjectId& projectId);

private:
    void persistCueOrder(const core::ProjectId& projectId, const std::vector<core::CueId>& cueOrder, sqlite3* handle);

    db::SqliteConnection& connection_;
};

}  // namespace projection::server::repo
