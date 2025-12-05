#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "projection/core/Cue.h"
#include "projection/core/Feed.h"
#include "projection/core/Scene.h"
#include "projection/core/Surface.h"

using namespace projection::core;

TEST_CASE("Surface validation enforces minimum vertices and parameter bounds", "[surface]") {
  Surface invalidVertices{makeSurfaceId("s1"), "Triangle", {}, makeFeedId("f1")};
  REQUIRE(!invalidVertices.isValid());

  std::vector<Vec2> triangle{{0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}};
  Surface validSurface{makeSurfaceId("s2"), "Valid", triangle, makeFeedId("f1"), 0.5f, 0.8f};
  REQUIRE(validSurface.isValid());

  Surface highOpacity{makeSurfaceId("s3"), "High", triangle, makeFeedId("f1"), 2.0f, 0.5f};
  REQUIRE(highOpacity.getOpacity() == 1.0f);
  REQUIRE(highOpacity.isValid());

  Surface negativeBrightness{makeSurfaceId("s4"), "Low", triangle, makeFeedId("f1"), 0.5f, -0.5f};
  REQUIRE(negativeBrightness.getBrightness() == 0.0f);
  REQUIRE(negativeBrightness.isValid());
}

TEST_CASE("Scene findSurface returns matching surfaces", "[scene]") {
  std::vector<Vec2> quad{{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};
  Surface surfaceA{makeSurfaceId("A"), "A", quad, makeFeedId("feed-1")};
  Surface surfaceB{makeSurfaceId("B"), "B", quad, makeFeedId("feed-2")};

  Scene scene{makeSceneId("scene"), "My Scene", "desc", {surfaceA, surfaceB}};

  REQUIRE(scene.findSurface(makeSurfaceId("A")) != nullptr);
  REQUIRE(scene.findSurface(makeSurfaceId("B")) != nullptr);
  REQUIRE(scene.findSurface(makeSurfaceId("missing")) == nullptr);

  Surface* mutableSurface = scene.findSurface(makeSurfaceId("A"));
  REQUIRE(mutableSurface != nullptr);
  mutableSurface->setName("Updated");
  REQUIRE(scene.findSurface(makeSurfaceId("A"))->getName() == "Updated");
}

TEST_CASE("Scene consistency validates surfaces and feed references", "[scene]") {
  Feed feed1{makeFeedId("feed-1"), "Camera", FeedType::Camera, "{}"};
  Feed feed2{makeFeedId("feed-2"), "Video", FeedType::VideoFile, "{}"};

  std::vector<Vec2> quad{{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};
  Surface surfaceGood{makeSurfaceId("A"), "A", quad, feed1.getId()};
  Surface surfaceMissingFeed{makeSurfaceId("B"), "B", quad, makeFeedId("missing")};

  Scene validScene{makeSceneId("scene-1"), "Valid", "", {surfaceGood}};
  REQUIRE(validScene.isConsistent({feed1, feed2}));

  Scene missingFeedScene{makeSceneId("scene-2"), "Invalid", "", {surfaceMissingFeed}};
  REQUIRE(!missingFeedScene.isConsistent({feed1}));

  Surface invalidSurface{makeSurfaceId("C"), "Invalid", {}, feed1.getId()};
  Scene invalidSurfaceScene{makeSceneId("scene-3"), "InvalidSurface", "", {invalidSurface}};
  REQUIRE(!invalidSurfaceScene.isConsistent({feed1}));
}

TEST_CASE("Cue stores references to scenes and surface parameter snapshots", "[cue]") {
  Cue cue{makeCueId("cue-1"), "Intro", makeSceneId("scene-1")};
  cue.getSurfaceOpacities()[makeSurfaceId("surface-1")] = 0.75f;
  cue.getSurfaceBrightnesses()[makeSurfaceId("surface-1")] = 0.6f;

  REQUIRE(cue.getSceneId() == makeSceneId("scene-1"));
  REQUIRE(cue.getSurfaceOpacities().at(makeSurfaceId("surface-1")) == 0.75f);
  REQUIRE(cue.getSurfaceBrightnesses().at(makeSurfaceId("surface-1")) == 0.6f);
}
