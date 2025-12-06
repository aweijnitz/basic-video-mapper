#pragma once

#include <string>

#include "projection/core/Enums.h"
#include "projection/core/Ids.h"

namespace projection::core {

struct VideoFileConfig {
  std::string filePath;
};

class Feed {
 public:
  Feed() = default;
  Feed(FeedId id, std::string name, FeedType type, std::string configJson);

  const FeedId& getId() const { return id_; }
  void setId(const FeedId& id) { id_ = id; }

  const std::string& getName() const { return name_; }
  void setName(const std::string& name) { name_ = name; }

  FeedType getType() const { return type_; }
  void setType(FeedType type) { type_ = type; }

  const std::string& getConfigJson() const { return configJson_; }
  void setConfigJson(const std::string& json) { configJson_ = json; }

  bool operator==(const Feed& other) const {
    return id_ == other.id_ && name_ == other.name_ && type_ == other.type_ && configJson_ == other.configJson_;
  }

 private:
  FeedId id_{};
  std::string name_{};
  FeedType type_{FeedType::VideoFile};
  std::string configJson_{};
};

VideoFileConfig parseVideoFileConfig(const Feed& feed);
Feed makeVideoFileFeed(const FeedId& id, const std::string& name, const std::string& filePath);

}  // namespace projection::core
