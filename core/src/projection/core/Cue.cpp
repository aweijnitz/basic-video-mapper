#include "projection/core/Cue.h"

#include <utility>

namespace projection::core {

Cue::Cue(CueId id, std::string name, SceneId sceneId)
    : id_(std::move(id)), name_(std::move(name)), sceneId_(std::move(sceneId)) {}

}  // namespace projection::core
