#include "projection/core/Surface.h"

#include <algorithm>
#include <utility>

namespace projection::core {

Surface::Surface(SurfaceId id, std::string name, std::vector<Vec2> vertices, FeedId feedId,
                 float opacity, float brightness, BlendMode blendMode, int zOrder)
    : id_(std::move(id)),
      name_(std::move(name)),
      vertices_(std::move(vertices)),
      feedId_(std::move(feedId)),
      opacity_(clampUnit(opacity)),
      brightness_(clampUnit(brightness)),
      blendMode_(blendMode),
      zOrder_(zOrder) {}

void Surface::setOpacity(float opacity) { opacity_ = clampUnit(opacity); }

void Surface::setBrightness(float brightness) { brightness_ = clampUnit(brightness); }

bool Surface::isValid() const {
  if (vertices_.size() < 3) {
    return false;
  }
  if (opacity_ < 0.0f || opacity_ > 1.0f) {
    return false;
  }
  if (brightness_ < 0.0f || brightness_ > 1.0f) {
    return false;
  }
  return true;
}

float Surface::clampUnit(float value) {
  return std::clamp(value, 0.0f, 1.0f);
}

}  // namespace projection::core
