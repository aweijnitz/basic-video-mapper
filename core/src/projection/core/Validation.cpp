#include "projection/core/Validation.h"

#include <algorithm>
#include <utility>

namespace projection::core {

bool validateSurface(const Surface& surface, std::string& errorMessage) {
  if (!surface.isValid()) {
    errorMessage = "Surface '" + surface.getId().value + "' is invalid.";
    return false;
  }

  errorMessage.clear();
  return true;
}

bool validateSceneFeeds(const Scene& scene, const std::vector<Feed>& feeds, std::string& errorMessage) {
  for (const auto& surface : scene.getSurfaces()) {
    if (!validateSurface(surface, errorMessage)) {
      return false;
    }

    const auto& feedId = surface.getFeedId();
    auto feedIt = std::find_if(feeds.begin(), feeds.end(), [&](const Feed& feed) {
      return feed.getId() == feedId;
    });

    if (feedIt == feeds.end()) {
      errorMessage = "Surface '" + surface.getId().value + "' references unknown feed '" + feedId.value + "'.";
      return false;
    }
  }

  errorMessage.clear();
  return true;
}

bool validateCueForScene(const Cue& cue, const Scene& scene, std::string& errorMessage) {
  if (cue.getSceneId() != scene.getId()) {
    errorMessage = "Cue '" + cue.getId().value + "' targets scene '" + cue.getSceneId().value +
                   "' which does not match scene '" + scene.getId().value + "'.";
    return false;
  }

  auto validateSurfaceReference = [&](const SurfaceId& surfaceId) {
    if (scene.findSurface(surfaceId) == nullptr) {
      errorMessage = "Cue references unknown surface '" + surfaceId.value + "' for scene '" + scene.getId().value + "'.";
      return false;
    }
    return true;
  };

  for (const auto& [surfaceId, _] : cue.getSurfaceOpacities()) {
    if (!validateSurfaceReference(surfaceId)) {
      return false;
    }
  }

  for (const auto& [surfaceId, _] : cue.getSurfaceBrightnesses()) {
    if (!validateSurfaceReference(surfaceId)) {
      return false;
    }
  }

  errorMessage.clear();
  return true;
}

}  // namespace projection::core

