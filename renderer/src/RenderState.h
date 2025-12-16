#pragma once

#include <unordered_map>
#include <vector>

#include <ofMain.h>

#include <projection/core/Feed.h>
#include <projection/core/Scene.h>

namespace projection::renderer {

struct VideoFeedResource {
  projection::core::FeedId id;
  ofVideoPlayer player;
  std::string filePath;
};

// Extracts the configured file paths for video feeds.
std::unordered_map<std::string, std::string> mapVideoFeedFilePaths(
    const projection::core::Scene& scene, const std::vector<projection::core::Feed>& feeds);

class RenderState {
 public:
  RenderState() = default;

  void loadSceneDefinition(const projection::core::Scene& scene,
                           const std::vector<projection::core::Feed>& feeds);
  void updateVideoPlayers();

  const projection::core::Scene& currentScene() const { return currentScene_; }
  const std::vector<projection::core::Feed>& currentFeeds() const { return currentFeeds_; }
  const std::unordered_map<std::string, VideoFeedResource>& videoFeeds() const { return videoFeeds_; }

 private:
  projection::core::Scene currentScene_{};
  std::vector<projection::core::Feed> currentFeeds_{};
  std::unordered_map<std::string, VideoFeedResource> videoFeeds_{};
};

}  // namespace projection::renderer
