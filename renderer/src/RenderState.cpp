#include "RenderState.h"

#include <projection/core/Feed.h>

using projection::core::Feed;
using projection::core::FeedType;
using projection::core::Scene;
using projection::core::VideoFileConfig;
using projection::core::parseVideoFileConfig;

namespace projection::renderer {

std::unordered_map<std::string, std::string> mapVideoFeedFilePaths(const Scene& /*scene*/,
                                                                   const std::vector<Feed>& feeds) {
  std::unordered_map<std::string, std::string> mapping;
  for (const auto& feed : feeds) {
    if (feed.getType() != FeedType::VideoFile) {
      continue;
    }
    VideoFileConfig config = parseVideoFileConfig(feed);
    mapping.emplace(feed.getId().value, config.filePath);
  }
  return mapping;
}

void RenderState::loadSceneDefinition(const Scene& scene, const std::vector<Feed>& feeds) {
  currentScene_ = scene;
  currentFeeds_ = feeds;
  videoFeeds_.clear();

  auto mapping = mapVideoFeedFilePaths(scene, feeds);
  for (const auto& feed : feeds) {
    if (feed.getType() != FeedType::VideoFile) {
      continue;
    }
    auto it = mapping.find(feed.getId().value);
    if (it == mapping.end()) {
      continue;
    }

    VideoFeedResource resource{feed.getId(), {}};
    const bool loaded = resource.player.load(it->second);
    if (loaded) {
      resource.player.setLoopState(OF_LOOP_NORMAL);
      resource.player.play();
    }
    resource.filePath = it->second;

    videoFeeds_.emplace(feed.getId().value, std::move(resource));
  }
}

void RenderState::updateVideoPlayers() {
  for (auto& entry : videoFeeds_) {
    entry.second.player.update();
  }
}

}  // namespace projection::renderer
