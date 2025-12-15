#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <functional>
#include <nlohmann/json.hpp>

#include "projection/core/Cue.h"
#include "projection/core/Feed.h"
#include "projection/core/Project.h"
#include "projection/core/Scene.h"
#include "projection/core/Serialization.h"
#include "projection/core/Surface.h"

using projection::core::BlendMode;
using projection::core::Cue;
using projection::core::CueId;
using projection::core::Feed;
using projection::core::FeedId;
using projection::core::FeedType;
using projection::core::Project;
using projection::core::ProjectId;
using projection::core::ProjectSettings;
using projection::core::Scene;
using projection::core::SceneId;
using projection::core::Surface;
using projection::core::SurfaceId;
using projection::core::Vec2;
using nlohmann::json;

namespace {

void expectRuntimeError(const std::function<void()>& fn) {
  bool threw = false;
  try {
    fn();
  } catch (const std::runtime_error&) {
    threw = true;
  }
  REQUIRE(threw);
}

}  // namespace

TEST_CASE("Feed round-trip serialization", "[serialization]") {
  Feed feed{FeedId{"feed-1"}, "Camera Feed", FeedType::Camera, "{\"device\":0}"};

  json j = feed;
  Feed parsed = j.get<Feed>();

  REQUIRE(parsed.getId().value == feed.getId().value);
  REQUIRE(parsed.getName() == feed.getName());
  REQUIRE(parsed.getType() == feed.getType());
  REQUIRE(parsed.getConfigJson() == feed.getConfigJson());
}

TEST_CASE("Surface round-trip serialization", "[serialization]") {
  std::vector<Vec2> verts{{0.f, 0.f}, {1.f, 0.f}, {1.f, 1.f}, {0.f, 1.f}};
  Surface surface{SurfaceId{"surface-1"}, "Quad", verts, FeedId{"feed-1"}, 0.8f, 0.9f,
                  BlendMode::Multiply, 2};

  json j = surface;
  Surface parsed = j.get<Surface>();

  REQUIRE(parsed.getId().value == surface.getId().value);
  REQUIRE(parsed.getName() == surface.getName());
  REQUIRE(parsed.getFeedId().value == surface.getFeedId().value);
  REQUIRE(std::abs(parsed.getOpacity() - surface.getOpacity()) < 1e-5f);
  REQUIRE(std::abs(parsed.getBrightness() - surface.getBrightness()) < 1e-5f);
  REQUIRE(parsed.getBlendMode() == surface.getBlendMode());
  REQUIRE(parsed.getZOrder() == surface.getZOrder());
  REQUIRE(parsed.getVertices().size() == surface.getVertices().size());
  for (size_t i = 0; i < parsed.getVertices().size(); ++i) {
    REQUIRE(std::abs(parsed.getVertices()[i].x - surface.getVertices()[i].x) < 1e-5f);
    REQUIRE(std::abs(parsed.getVertices()[i].y - surface.getVertices()[i].y) < 1e-5f);
  }
}

TEST_CASE("Scene round-trip serialization", "[serialization]") {
  std::vector<Vec2> verts{{0.f, 0.f}, {2.f, 0.f}, {2.f, 2.f}, {0.f, 2.f}};
  Surface surfaceA{SurfaceId{"sA"}, "Left", verts, FeedId{"feedA"}, 1.0f, 1.0f, BlendMode::Normal, 0};
  Surface surfaceB{SurfaceId{"sB"}, "Right", verts, FeedId{"feedB"}, 0.7f, 0.8f, BlendMode::Additive, 1};
  Scene scene{SceneId{"scene-123"}, "Main Scene", "Two quads", {surfaceA, surfaceB}};

  json j = scene;
  Scene parsed = j.get<Scene>();

  REQUIRE(parsed.getId().value == scene.getId().value);
  REQUIRE(parsed.getName() == scene.getName());
  REQUIRE(parsed.getDescription() == scene.getDescription());
  REQUIRE(parsed.getSurfaces().size() == scene.getSurfaces().size());
}

TEST_CASE("Cue round-trip serialization", "[serialization]") {
  Cue cue{CueId{"cue-1"}, "Intro", SceneId{"scene-123"}};
  cue.getSurfaceOpacities()[SurfaceId{"sA"}] = 0.5f;
  cue.getSurfaceBrightnesses()[SurfaceId{"sA"}] = 0.7f;
  cue.getSurfaceOpacities()[SurfaceId{"sB"}] = 1.0f;
  cue.getSurfaceBrightnesses()[SurfaceId{"sB"}] = 1.2f;

  json j = cue;
  Cue parsed = j.get<Cue>();

  REQUIRE(parsed.getId().value == cue.getId().value);
  REQUIRE(parsed.getName() == cue.getName());
  REQUIRE(parsed.getSceneId().value == cue.getSceneId().value);
  REQUIRE(parsed.getSurfaceOpacities().size() == cue.getSurfaceOpacities().size());
  REQUIRE(parsed.getSurfaceBrightnesses().size() == cue.getSurfaceBrightnesses().size());
  for (const auto& [surfaceId, opacity] : cue.getSurfaceOpacities()) {
    REQUIRE(std::abs(parsed.getSurfaceOpacities().at(surfaceId) - opacity) < 1e-5f);
  }
  for (const auto& [surfaceId, brightness] : cue.getSurfaceBrightnesses()) {
    REQUIRE(std::abs(parsed.getSurfaceBrightnesses().at(surfaceId) - brightness) < 1e-5f);
  }
}

TEST_CASE("Project round-trip serialization", "[serialization]") {
  ProjectSettings settings;
  settings.controllers["fader1"] = "masterBrightness";
  settings.midiChannels = {1, 10};
  settings.globalConfig["clockBpm"] = "128";

  Project project{ProjectId{"proj-1"},
                  "Main Show",
                  "Demo project",
                  {CueId{"cue-1"}, CueId{"cue-2"}},
                  settings};

  json j = project;
  Project parsed = j.get<Project>();

  REQUIRE(parsed.getId().value == project.getId().value);
  REQUIRE(parsed.getName() == project.getName());
  REQUIRE(parsed.getDescription() == project.getDescription());
  REQUIRE(parsed.getCueOrder().size() == project.getCueOrder().size());
  REQUIRE(parsed.getCueOrder()[0].value == "cue-1");
  REQUIRE(parsed.getSettings().controllers.at("fader1") == "masterBrightness");
  std::vector<int> expectedChannels{1, 10};
  REQUIRE(parsed.getSettings().midiChannels == expectedChannels);
  REQUIRE(parsed.getSettings().globalConfig.at("clockBpm") == "128");
}

TEST_CASE("Feed configJson accepts string or object", "[serialization]") {
  json feedWithString = {
      {"id", "feed-1"},
      {"name", "Clip"},
      {"type", "VideoFile"},
      {"configJson", "{\"filePath\":\"data/assets/clipA.mp4\"}"},
  };
  Feed parsedString = feedWithString.get<Feed>();
  REQUIRE(parsedString.getConfigJson() == "{\"filePath\":\"data/assets/clipA.mp4\"}");

  json feedWithObject = {
      {"id", "feed-2"},
      {"name", "Clip"},
      {"type", "VideoFile"},
      {"configJson", json{{"filePath", "data/assets/clipB.mp4"}}},
  };
  Feed parsedObject = feedWithObject.get<Feed>();
  REQUIRE(parsedObject.getConfigJson() == "{\"filePath\":\"data/assets/clipB.mp4\"}");
}

TEST_CASE("Invalid enum strings throw", "[serialization][negative]") {
  json invalidFeed = {{"id", "feed-1"},
                      {"name", "Invalid"},
                      {"type", "NotAType"},
                      {"configJson", "{}"}};
  expectRuntimeError([&]() { invalidFeed.get<Feed>(); });

  json invalidSurface = {{"id", "s1"},
                         {"name", "Surf"},
                         {"vertices", json::array({{{"x", 0}, {"y", 0}}})},
                         {"feedId", "feed"},
                         {"opacity", 1.0},
                         {"brightness", 1.0},
                         {"blendMode", "BadMode"},
                         {"zOrder", 0}};
  expectRuntimeError([&]() { invalidSurface.get<Surface>(); });
}

TEST_CASE("Missing required fields throw", "[serialization][negative]") {
  json missingId = {{"name", "No Id"}, {"type", "VideoFile"}, {"configJson", "{}"}};
  expectRuntimeError([&]() { missingId.get<Feed>(); });

  json missingVertices = {{"id", "s1"},
                          {"name", "Surf"},
                          {"feedId", "feed"},
                          {"opacity", 1.0},
                          {"brightness", 1.0},
                          {"blendMode", "Normal"},
                          {"zOrder", 0}};
  expectRuntimeError([&]() { missingVertices.get<Surface>(); });
}

TEST_CASE("Type mismatches throw", "[serialization][negative]") {
  json wrongType = {{"id", 123},
                    {"name", "Bad"},
                    {"type", "VideoFile"},
                    {"configJson", "{}"}};
  expectRuntimeError([&]() { wrongType.get<Feed>(); });

  json badVertices = {{"id", "s1"},
                      {"name", "Surf"},
                      {"vertices", json::array({{"x", 0}, {"y", 0}})},
                      {"feedId", "feed"},
                      {"opacity", "one"},
                      {"brightness", 1.0},
                      {"blendMode", "Normal"},
                      {"zOrder", 0}};
  expectRuntimeError([&]() { badVertices.get<Surface>(); });

  json badProject = {
      {"id", "proj-1"},
      {"name", "Bad"},
      {"description", "desc"},
      {"cueOrder", json::array({{"not-a-string"}})},
      {"settings", json{{"midiChannels", json::array({1, 2})}}},
  };
  expectRuntimeError([&]() { badProject.get<Project>(); });
}
