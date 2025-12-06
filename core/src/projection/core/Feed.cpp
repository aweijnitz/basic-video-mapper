#include "projection/core/Feed.h"

#include <nlohmann/json.hpp>
#include <utility>
#include <stdexcept>

namespace projection::core {

Feed::Feed(FeedId id, std::string name, FeedType type, std::string configJson)
    : id_(std::move(id)),
      name_(std::move(name)),
      type_(type),
      configJson_(std::move(configJson)) {}

VideoFileConfig parseVideoFileConfig(const Feed& feed) {
  if (feed.getType() != FeedType::VideoFile) {
    throw std::runtime_error("parseVideoFileConfig requires a VideoFile feed");
  }

  auto json = nlohmann::json::parse(feed.getConfigJson());
  if (!json.is_object() || !json.contains("filePath") || !json["filePath"].is_string()) {
    throw std::runtime_error("Invalid VideoFile feed config: missing filePath");
  }

  return VideoFileConfig{json["filePath"].get<std::string>()};
}

Feed makeVideoFileFeed(const FeedId& id, const std::string& name, const std::string& filePath) {
  nlohmann::json config{{"filePath", filePath}};
  return Feed(id, name, FeedType::VideoFile, config.dump());
}

}  // namespace projection::core
