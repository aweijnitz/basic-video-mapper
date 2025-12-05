#pragma once

#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

#include "projection/core/Cue.h"
#include "projection/core/Enums.h"
#include "projection/core/Feed.h"
#include "projection/core/Scene.h"
#include "projection/core/Surface.h"

namespace projection::core {

// JSON serialization helpers for core domain types.
//
// Error handling strategy: deserialization functions throw std::runtime_error when JSON is
// missing required fields, has the wrong type, or contains invalid enum strings.

void to_json(nlohmann::json& j, const FeedType& type);
void from_json(const nlohmann::json& j, FeedType& type);

void to_json(nlohmann::json& j, const BlendMode& mode);
void from_json(const nlohmann::json& j, BlendMode& mode);

void to_json(nlohmann::json& j, const Vec2& vec);
void from_json(const nlohmann::json& j, Vec2& vec);

void to_json(nlohmann::json& j, const Feed& feed);
void from_json(const nlohmann::json& j, Feed& feed);

void to_json(nlohmann::json& j, const Surface& surface);
void from_json(const nlohmann::json& j, Surface& surface);

void to_json(nlohmann::json& j, const Scene& scene);
void from_json(const nlohmann::json& j, Scene& scene);

void to_json(nlohmann::json& j, const Cue& cue);
void from_json(const nlohmann::json& j, Cue& cue);

}  // namespace projection::core

