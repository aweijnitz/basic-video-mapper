#pragma once

#include <string>

namespace projection::core {

// Strongly-typed identifiers for core domain entities.
struct SceneId {
  std::string value;

  explicit SceneId(std::string v = "") : value(std::move(v)) {}

  bool operator==(const SceneId& other) const { return value == other.value; }
  bool operator!=(const SceneId& other) const { return !(*this == other); }
  bool operator<(const SceneId& other) const { return value < other.value; }
};

struct SurfaceId {
  std::string value;

  explicit SurfaceId(std::string v = "") : value(std::move(v)) {}

  bool operator==(const SurfaceId& other) const { return value == other.value; }
  bool operator!=(const SurfaceId& other) const { return !(*this == other); }
  bool operator<(const SurfaceId& other) const { return value < other.value; }
};

struct FeedId {
  std::string value;

  explicit FeedId(std::string v = "") : value(std::move(v)) {}

  bool operator==(const FeedId& other) const { return value == other.value; }
  bool operator!=(const FeedId& other) const { return !(*this == other); }
  bool operator<(const FeedId& other) const { return value < other.value; }
};

struct CueId {
  std::string value;

  explicit CueId(std::string v = "") : value(std::move(v)) {}

  bool operator==(const CueId& other) const { return value == other.value; }
  bool operator!=(const CueId& other) const { return !(*this == other); }
  bool operator<(const CueId& other) const { return value < other.value; }
};

inline SceneId makeSceneId(const std::string& raw) { return SceneId(raw); }
inline SurfaceId makeSurfaceId(const std::string& raw) { return SurfaceId(raw); }
inline FeedId makeFeedId(const std::string& raw) { return FeedId(raw); }
inline CueId makeCueId(const std::string& raw) { return CueId(raw); }

}  // namespace projection::core
