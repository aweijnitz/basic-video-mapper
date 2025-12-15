#pragma once

#include <vector>

#include "db/SqliteConnection.h"
#include "projection/core/Scene.h"
#include "projection/core/Surface.h"

namespace projection::server::repo {

class SurfaceRepository {
public:
    explicit SurfaceRepository(db::SqliteConnection& connection);

    core::Surface createSurface(const core::Surface& surface, const core::SceneId& sceneId);

    std::vector<core::Surface> listSurfacesForScene(const core::SceneId& sceneId);
    void deleteSurfacesForScene(const core::SceneId& sceneId);

private:
    db::SqliteConnection& connection_;
};

}  // namespace projection::server::repo
