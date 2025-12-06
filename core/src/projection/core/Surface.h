#pragma once

#include <string>
#include <vector>

#include "projection/core/Enums.h"
#include "projection/core/Feed.h"
#include "projection/core/Ids.h"

namespace projection::core {

struct Vec2 {
  float x{0.0f};
  float y{0.0f};

  bool operator==(const Vec2& other) const { return x == other.x && y == other.y; }
};

class Surface {
 public:
  Surface() = default;
  Surface(SurfaceId id, std::string name, std::vector<Vec2> vertices, FeedId feedId,
          float opacity = 1.0f, float brightness = 1.0f, BlendMode blendMode = BlendMode::Normal,
          int zOrder = 0);

  const SurfaceId& getId() const { return id_; }
  void setId(const SurfaceId& id) { id_ = id; }

  const std::string& getName() const { return name_; }
  void setName(const std::string& name) { name_ = name; }

  const std::vector<Vec2>& getVertices() const { return vertices_; }
  void setVertices(const std::vector<Vec2>& vertices) { vertices_ = vertices; }

  const FeedId& getFeedId() const { return feedId_; }
  void setFeedId(const FeedId& feedId) { feedId_ = feedId; }

  float getOpacity() const { return opacity_; }
  void setOpacity(float opacity);

  float getBrightness() const { return brightness_; }
  void setBrightness(float brightness);

  BlendMode getBlendMode() const { return blendMode_; }
  void setBlendMode(BlendMode mode) { blendMode_ = mode; }

  int getZOrder() const { return zOrder_; }
  void setZOrder(int z) { zOrder_ = z; }

  bool isValid() const;

  bool operator==(const Surface& other) const {
    return id_ == other.id_ && name_ == other.name_ && vertices_ == other.vertices_ && feedId_ == other.feedId_ &&
           opacity_ == other.opacity_ && brightness_ == other.brightness_ && blendMode_ == other.blendMode_ &&
           zOrder_ == other.zOrder_;
  }

 private:
  static float clampUnit(float value);

  SurfaceId id_{};
  std::string name_{};
  std::vector<Vec2> vertices_{};
  FeedId feedId_{};
  float opacity_{1.0f};
  float brightness_{1.0f};
  BlendMode blendMode_{BlendMode::Normal};
  int zOrder_{0};
};

}  // namespace projection::core
