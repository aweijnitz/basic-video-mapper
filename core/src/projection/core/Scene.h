#pragma once

#include <string>
#include <vector>

#include "projection/core/Feed.h"
#include "projection/core/Ids.h"
#include "projection/core/Surface.h"

namespace projection::core {

class Scene {
 public:
  Scene() = default;
  Scene(SceneId id, std::string name, std::string description, std::vector<Surface> surfaces);

  const SceneId& getId() const { return id_; }
  void setId(const SceneId& id) { id_ = id; }

  const std::string& getName() const { return name_; }
  void setName(const std::string& name) { name_ = name; }

  const std::string& getDescription() const { return description_; }
  void setDescription(const std::string& description) { description_ = description; }

  const std::vector<Surface>& getSurfaces() const { return surfaces_; }
  std::vector<Surface>& getSurfaces() { return surfaces_; }
  void setSurfaces(const std::vector<Surface>& surfaces) { surfaces_ = surfaces; }

  const Surface* findSurface(const SurfaceId& id) const;
  Surface* findSurface(const SurfaceId& id);

  bool isConsistent(const std::vector<Feed>& feeds) const;

  bool operator==(const Scene& other) const {
    return id_ == other.id_ && name_ == other.name_ && description_ == other.description_ &&
           surfaces_ == other.surfaces_;
  }

 private:
  SceneId id_{};
  std::string name_{};
  std::string description_{};
  std::vector<Surface> surfaces_{};
};

}  // namespace projection::core
