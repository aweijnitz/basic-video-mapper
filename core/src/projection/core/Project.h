#pragma once

#include <map>
#include <string>
#include <vector>

#include "projection/core/Ids.h"

namespace projection::core {

struct ProjectSettings {
  std::map<std::string, std::string> controllers{};
  std::vector<int> midiChannels{};
  std::map<std::string, std::string> globalConfig{};
};

class Project {
 public:
  Project() = default;
  Project(ProjectId id, std::string name, std::string description, std::vector<CueId> cueOrder,
          ProjectSettings settings = {});

  const ProjectId& getId() const { return id_; }
  void setId(const ProjectId& id) { id_ = id; }

  const std::string& getName() const { return name_; }
  void setName(const std::string& name) { name_ = name; }

  const std::string& getDescription() const { return description_; }
  void setDescription(const std::string& description) { description_ = description; }

  const std::vector<CueId>& getCueOrder() const { return cueOrder_; }
  std::vector<CueId>& getCueOrder() { return cueOrder_; }
  void setCueOrder(const std::vector<CueId>& cueOrder) { cueOrder_ = cueOrder; }

  const ProjectSettings& getSettings() const { return settings_; }
  ProjectSettings& getSettings() { return settings_; }
  void setSettings(const ProjectSettings& settings) { settings_ = settings; }

 private:
  ProjectId id_{};
  std::string name_{};
  std::string description_{};
  std::vector<CueId> cueOrder_{};
  ProjectSettings settings_{};
};

}  // namespace projection::core
