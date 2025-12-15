#include "projection/core/Project.h"

namespace projection::core {

Project::Project(ProjectId id, std::string name, std::string description, std::vector<CueId> cueOrder,
                 ProjectSettings settings)
    : id_(std::move(id)),
      name_(std::move(name)),
      description_(std::move(description)),
      cueOrder_(std::move(cueOrder)),
      settings_(std::move(settings)) {}

}  // namespace projection::core
