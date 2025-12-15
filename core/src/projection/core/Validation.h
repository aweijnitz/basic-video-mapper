#pragma once

#include <string>
#include <vector>

#include "projection/core/Cue.h"
#include "projection/core/Feed.h"
#include "projection/core/Project.h"
#include "projection/core/Scene.h"
#include "projection/core/Surface.h"

namespace projection::core {

// Validates a single surface instance and returns a descriptive error on failure.
bool validateSurface(const Surface& surface, std::string& errorMessage);

// Ensures that all surfaces in the scene are valid and reference feeds that exist in the provided list.
bool validateSceneFeeds(const Scene& scene, const std::vector<Feed>& feeds, std::string& errorMessage);

// Ensures that a cue refers to the provided scene and only references surfaces contained in that scene.
bool validateCueForScene(const Cue& cue, const Scene& scene, std::string& errorMessage);

// Ensures that all cues referenced by the project exist in the provided list and preserves ordering.
bool validateProjectCues(const Project& project, const std::vector<Cue>& cues, std::string& errorMessage);

}  // namespace projection::core
