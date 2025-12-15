#include "projection/core/Serialization.h"

#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using nlohmann::json;

namespace projection::core {
namespace {

template <typename T>
const T& requireField(const json& j, const std::string& key) {
  if (!j.contains(key)) {
    throw std::runtime_error("Missing required field: " + key);
  }
  return j.at(key);
}

float requireNumber(const json& j, const std::string& key) {
  const auto& value = requireField<json>(j, key);
  if (!value.is_number()) {
    throw std::runtime_error("Field '" + key + "' must be a number");
  }
  return value.get<float>();
}

int requireInteger(const json& j, const std::string& key) {
  const auto& value = requireField<json>(j, key);
  if (!value.is_number_integer()) {
    throw std::runtime_error("Field '" + key + "' must be an integer");
  }
  return value.get<int>();
}

std::string requireString(const json& j, const std::string& key) {
  const auto& value = requireField<json>(j, key);
  if (!value.is_string()) {
    throw std::runtime_error("Field '" + key + "' must be a string");
  }
  return value.get<std::string>();
}

FeedType parseFeedTypeString(const std::string& raw) {
  FeedType type{};
  if (!fromString(raw, type)) {
    throw std::runtime_error("Invalid FeedType: " + raw);
  }
  return type;
}

BlendMode parseBlendModeString(const std::string& raw) {
  BlendMode mode{};
  if (!fromString(raw, mode)) {
    throw std::runtime_error("Invalid BlendMode: " + raw);
  }
  return mode;
}

std::map<SurfaceId, float> readSurfaceValueArray(const json& array, const std::string& field) {
  if (!array.is_array()) {
    throw std::runtime_error("Field '" + field + "' must be an array");
  }
  std::map<SurfaceId, float> result;
  for (const auto& entry : array) {
    if (!entry.is_object()) {
      throw std::runtime_error("Entries in '" + field + "' must be objects");
    }
    auto surfaceId = requireString(entry, "surfaceId");
    const auto value = requireNumber(entry, "value");
    result.emplace(SurfaceId(surfaceId), value);
  }
  return result;
}

json surfaceValueArray(const std::map<SurfaceId, float>& values) {
  json arr = json::array();
  for (const auto& [id, value] : values) {
    arr.push_back({{"surfaceId", id.value}, {"value", value}});
  }
  return arr;
}

}  // namespace

void to_json(json& j, const FeedType& type) { j = toString(type); }

void from_json(const json& j, FeedType& type) {
  if (!j.is_string()) {
    throw std::runtime_error("FeedType must be a string");
  }
  type = parseFeedTypeString(j.get<std::string>());
}

void to_json(json& j, const BlendMode& mode) { j = toString(mode); }

void from_json(const json& j, BlendMode& mode) {
  if (!j.is_string()) {
    throw std::runtime_error("BlendMode must be a string");
  }
  mode = parseBlendModeString(j.get<std::string>());
}

void to_json(json& j, const VideoFileConfig& config) { j = json{{"filePath", config.filePath}}; }

void from_json(const json& j, VideoFileConfig& config) {
  if (!j.is_object()) {
    throw std::runtime_error("VideoFileConfig must be an object");
  }
  config.filePath = requireString(j, "filePath");
}

void to_json(json& j, const Vec2& vec) { j = json{{"x", vec.x}, {"y", vec.y}}; }

void from_json(const json& j, Vec2& vec) {
  if (!j.is_object()) {
    throw std::runtime_error("Vec2 must be an object");
  }
  vec.x = requireNumber(j, "x");
  vec.y = requireNumber(j, "y");
}

void to_json(json& j, const Feed& feed) {
  j = json{{"id", feed.getId().value},
           {"name", feed.getName()},
           {"type", feed.getType()},
           {"configJson", feed.getConfigJson()}};
}

void from_json(const json& j, Feed& feed) {
  if (!j.is_object()) {
    throw std::runtime_error("Feed must be an object");
  }
  const auto id = requireString(j, "id");
  const auto name = requireString(j, "name");
  const auto typeStr = requireString(j, "type");
  const auto& configField = requireField<json>(j, "configJson");
  std::string config;
  if (configField.is_string()) {
    config = configField.get<std::string>();
  } else if (configField.is_object() || configField.is_array()) {
    // Accept a nested JSON object/array and store its serialized form
    config = configField.dump();
  } else {
    throw std::runtime_error("Field 'configJson' must be a string or object");
  }

  feed = Feed(FeedId{id}, name, parseFeedTypeString(typeStr), config);
}

void to_json(json& j, const Surface& surface) {
  j = json{{"id", surface.getId().value},
           {"name", surface.getName()},
           {"vertices", surface.getVertices()},
           {"feedId", surface.getFeedId().value},
           {"opacity", surface.getOpacity()},
           {"brightness", surface.getBrightness()},
           {"blendMode", surface.getBlendMode()},
           {"zOrder", surface.getZOrder()}};
}

void from_json(const json& j, Surface& surface) {
  if (!j.is_object()) {
    throw std::runtime_error("Surface must be an object");
  }

  const auto id = requireString(j, "id");
  const auto name = requireString(j, "name");
  const auto feedId = requireString(j, "feedId");
  const auto opacity = requireNumber(j, "opacity");
  const auto brightness = requireNumber(j, "brightness");
  const auto blendModeStr = requireString(j, "blendMode");

  const auto& verticesJson = requireField<json>(j, "vertices");
  if (!verticesJson.is_array()) {
    throw std::runtime_error("Field 'vertices' must be an array");
  }
  std::vector<Vec2> vertices;
  vertices.reserve(verticesJson.size());
  for (const auto& vertexJson : verticesJson) {
    Vec2 vec{};
    from_json(vertexJson, vec);
    vertices.push_back(vec);
  }

  surface = Surface(SurfaceId{id}, name, vertices, FeedId{feedId}, opacity, brightness,
                    parseBlendModeString(blendModeStr), requireInteger(j, "zOrder"));
}

void to_json(json& j, const Scene& scene) {
  j = json{{"id", scene.getId().value},
           {"name", scene.getName()},
           {"description", scene.getDescription()},
           {"surfaces", scene.getSurfaces()}};
}

void from_json(const json& j, Scene& scene) {
  if (!j.is_object()) {
    throw std::runtime_error("Scene must be an object");
  }
  const auto id = requireString(j, "id");
  const auto name = requireString(j, "name");
  const auto description = requireString(j, "description");
  const auto& surfacesJson = requireField<json>(j, "surfaces");
  if (!surfacesJson.is_array()) {
    throw std::runtime_error("Field 'surfaces' must be an array");
  }

  std::vector<Surface> surfaces;
  surfaces.reserve(surfacesJson.size());
  for (const auto& surfaceJson : surfacesJson) {
    Surface surfaceInstance;
    from_json(surfaceJson, surfaceInstance);
    surfaces.push_back(surfaceInstance);
  }

  scene = Scene(SceneId{id}, name, description, surfaces);
}

void to_json(json& j, const Cue& cue) {
  j = json{{"id", cue.getId().value},
           {"name", cue.getName()},
           {"sceneId", cue.getSceneId().value},
           {"surfaceOpacities", surfaceValueArray(cue.getSurfaceOpacities())},
           {"surfaceBrightnesses", surfaceValueArray(cue.getSurfaceBrightnesses())}};
}

void from_json(const json& j, Cue& cue) {
  if (!j.is_object()) {
    throw std::runtime_error("Cue must be an object");
  }
  const auto id = requireString(j, "id");
  const auto name = requireString(j, "name");
  const auto sceneId = requireString(j, "sceneId");

  const auto& opacitiesJson = requireField<json>(j, "surfaceOpacities");
  const auto& brightnessesJson = requireField<json>(j, "surfaceBrightnesses");

  auto opacities = readSurfaceValueArray(opacitiesJson, "surfaceOpacities");
  auto brightnesses = readSurfaceValueArray(brightnessesJson, "surfaceBrightnesses");

  cue = Cue(CueId{id}, name, SceneId{sceneId});
  cue.getSurfaceOpacities() = std::move(opacities);
  cue.getSurfaceBrightnesses() = std::move(brightnesses);
}

}  // namespace projection::core
