#include "projection/core/Scene.h"

#include <algorithm>
#include <utility>

namespace projection::core {

Scene::Scene(SceneId id, std::string name, std::string description, std::vector<Surface> surfaces)
    : id_(std::move(id)),
      name_(std::move(name)),
      description_(std::move(description)),
      surfaces_(std::move(surfaces)) {}

const Surface* Scene::findSurface(const SurfaceId& id) const {
  for (const auto& surface : surfaces_) {
    if (surface.getId() == id) {
      return &surface;
    }
  }
  return nullptr;
}

Surface* Scene::findSurface(const SurfaceId& id) {
  for (auto& surface : surfaces_) {
    if (surface.getId() == id) {
      return &surface;
    }
  }
  return nullptr;
}

bool Scene::isConsistent(const std::vector<Feed>& feeds) const {
  for (const auto& surface : surfaces_) {
    if (!surface.isValid()) {
      return false;
    }

    const auto& feedId = surface.getFeedId();
    auto feedIt = std::find_if(feeds.begin(), feeds.end(), [&](const Feed& feed) {
      return feed.getId() == feedId;
    });
    if (feedIt == feeds.end()) {
      return false;
    }
  }
  return true;
}

}  // namespace projection::core
