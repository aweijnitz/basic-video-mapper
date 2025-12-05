#pragma once

#include <string>

namespace projection::core {

enum class FeedType { VideoFile, Camera, Generated };

enum class BlendMode { Normal, Additive, Multiply };

// Convert FeedType to a readable string representation.
inline std::string toString(FeedType type) {
  switch (type) {
    case FeedType::VideoFile:
      return "VideoFile";
    case FeedType::Camera:
      return "Camera";
    case FeedType::Generated:
      return "Generated";
  }
  return "Unknown";
}

// Parse a FeedType from a string. Returns true on success.
inline bool fromString(const std::string& value, FeedType& outType) {
  if (value == "VideoFile") {
    outType = FeedType::VideoFile;
    return true;
  }
  if (value == "Camera") {
    outType = FeedType::Camera;
    return true;
  }
  if (value == "Generated") {
    outType = FeedType::Generated;
    return true;
  }
  return false;
}

// Convert BlendMode to a readable string representation.
inline std::string toString(BlendMode mode) {
  switch (mode) {
    case BlendMode::Normal:
      return "Normal";
    case BlendMode::Additive:
      return "Additive";
    case BlendMode::Multiply:
      return "Multiply";
  }
  return "Unknown";
}

// Parse a BlendMode from a string. Returns true on success.
inline bool fromString(const std::string& value, BlendMode& outMode) {
  if (value == "Normal") {
    outMode = BlendMode::Normal;
    return true;
  }
  if (value == "Additive") {
    outMode = BlendMode::Additive;
    return true;
  }
  if (value == "Multiply") {
    outMode = BlendMode::Multiply;
    return true;
  }
  return false;
}

}  // namespace projection::core
