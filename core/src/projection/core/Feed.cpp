#include "projection/core/Feed.h"

#include <utility>

namespace projection::core {

Feed::Feed(FeedId id, std::string name, FeedType type, std::string configJson)
    : id_(std::move(id)),
      name_(std::move(name)),
      type_(type),
      configJson_(std::move(configJson)) {}

}  // namespace projection::core
