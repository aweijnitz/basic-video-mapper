#pragma once

#include <vector>
#include <optional>

#include "db/SqliteConnection.h"
#include "projection/core/Scene.h"

namespace projection::server::repo {

// Repository responsible for persisting and retrieving Scene objects.
// All functions throw std::runtime_error on SQL errors or invalid data.
class SceneRepository {
public:
    explicit SceneRepository(db::SqliteConnection& connection);

    core::Scene createScene(const core::Scene& scene);

    std::vector<core::Scene> listScenes();

    std::optional<core::Scene> findSceneById(const core::SceneId& sceneId);

    bool sceneExists(const core::SceneId& sceneId);

private:
    db::SqliteConnection& connection_;
};

}  // namespace projection::server::repo
