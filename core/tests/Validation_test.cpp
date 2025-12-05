#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "projection/core/Cue.h"
#include "projection/core/Feed.h"
#include "projection/core/Scene.h"
#include "projection/core/Surface.h"
#include "projection/core/Validation.h"

using namespace projection::core;

TEST_CASE("validateSceneFeeds succeeds when surfaces and feeds are consistent", "[validation]") {
  Feed feed{makeFeedId("feed-1"), "Camera", FeedType::Camera, "{}"};
  std::vector<Vec2> quad{{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};
  Surface surface{makeSurfaceId("surface-1"), "Quad", quad, feed.getId()};
  Scene scene{makeSceneId("scene-1"), "Scene", "", {surface}};

  std::string errorMessage;
  REQUIRE(validateSceneFeeds(scene, {feed}, errorMessage));
  REQUIRE(errorMessage.empty());
}

TEST_CASE("validateSceneFeeds fails when surface is invalid", "[validation]") {
  Feed feed{makeFeedId("feed-1"), "Camera", FeedType::Camera, "{}"};
  Surface invalidSurface{makeSurfaceId("bad"), "Invalid", {}, feed.getId()};
  Scene scene{makeSceneId("scene-1"), "Scene", "", {invalidSurface}};
  std::string errorMessage;

  REQUIRE(!validateSceneFeeds(scene, {feed}, errorMessage));
  REQUIRE(!errorMessage.empty());
}

TEST_CASE("validateSceneFeeds fails when feed reference is missing", "[validation]") {
  Feed feed{makeFeedId("feed-1"), "Camera", FeedType::Camera, "{}"};
  std::vector<Vec2> triangle{{0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}};
  Surface missingFeedSurface{makeSurfaceId("missing-feed"), "Missing", triangle, makeFeedId("other")};
  Scene scene{makeSceneId("scene-1"), "Scene", "", {missingFeedSurface}};
  std::string errorMessage;

  REQUIRE(!validateSceneFeeds(scene, {feed}, errorMessage));
  REQUIRE(!errorMessage.empty());
}

TEST_CASE("validateSceneFeeds succeeds for empty scene regardless of feeds", "[validation]") {
  Scene emptyScene{makeSceneId("scene-empty"), "Empty", "", {}};
  std::string errorMessage;

  REQUIRE(validateSceneFeeds(emptyScene, {}, errorMessage));
  REQUIRE(errorMessage.empty());
}

TEST_CASE("validateCueForScene ensures scene matches and surfaces exist", "[validation]") {
  Feed feed{makeFeedId("feed-1"), "Camera", FeedType::Camera, "{}"};
  std::vector<Vec2> quad{{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};
  Surface surfaceA{makeSurfaceId("A"), "A", quad, feed.getId()};
  Surface surfaceB{makeSurfaceId("B"), "B", quad, feed.getId()};
  Scene scene{makeSceneId("scene-1"), "Scene", "", {surfaceA, surfaceB}};

  Cue cue{makeCueId("cue-1"), "Cue", scene.getId()};
  cue.getSurfaceOpacities()[surfaceA.getId()] = 0.5f;
  cue.getSurfaceBrightnesses()[surfaceB.getId()] = 0.75f;

  std::string errorMessage;
  REQUIRE(validateCueForScene(cue, scene, errorMessage));
  REQUIRE(errorMessage.empty());
}

TEST_CASE("validateCueForScene fails when scenes do not match", "[validation]") {
  Feed feed{makeFeedId("feed-1"), "Camera", FeedType::Camera, "{}"};
  std::vector<Vec2> quad{{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};
  Surface surfaceA{makeSurfaceId("A"), "A", quad, feed.getId()};
  Scene scene{makeSceneId("scene-1"), "Scene", "", {surfaceA}};
  Cue cue{makeCueId("cue-1"), "Cue", makeSceneId("other-scene")};
  std::string errorMessage;

  REQUIRE(!validateCueForScene(cue, scene, errorMessage));
  REQUIRE(!errorMessage.empty());
}

TEST_CASE("validateCueForScene fails when opacity map references missing surface", "[validation]") {
  Feed feed{makeFeedId("feed-1"), "Camera", FeedType::Camera, "{}"};
  std::vector<Vec2> quad{{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};
  Surface surfaceA{makeSurfaceId("A"), "A", quad, feed.getId()};
  Scene scene{makeSceneId("scene-1"), "Scene", "", {surfaceA}};
  Cue cue{makeCueId("cue-1"), "Cue", scene.getId()};
  cue.getSurfaceOpacities()[makeSurfaceId("missing")] = 0.5f;
  std::string errorMessage;

  REQUIRE(!validateCueForScene(cue, scene, errorMessage));
  REQUIRE(!errorMessage.empty());
}

TEST_CASE("validateCueForScene fails when brightness map references missing surface", "[validation]") {
  Feed feed{makeFeedId("feed-1"), "Camera", FeedType::Camera, "{}"};
  std::vector<Vec2> quad{{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};
  Surface surfaceA{makeSurfaceId("A"), "A", quad, feed.getId()};
  Scene scene{makeSceneId("scene-1"), "Scene", "", {surfaceA}};
  Cue cue{makeCueId("cue-1"), "Cue", scene.getId()};
  cue.getSurfaceBrightnesses()[makeSurfaceId("missing")] = 0.5f;
  std::string errorMessage;

  REQUIRE(!validateCueForScene(cue, scene, errorMessage));
  REQUIRE(!errorMessage.empty());
}

TEST_CASE("validateCueForScene succeeds for cue with no surface entries", "[validation]") {
  Feed feed{makeFeedId("feed-1"), "Camera", FeedType::Camera, "{}"};
  std::vector<Vec2> quad{{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};
  Surface surfaceA{makeSurfaceId("A"), "A", quad, feed.getId()};
  Scene scene{makeSceneId("scene-1"), "Scene", "", {surfaceA}};
  Cue cue{makeCueId("cue-1"), "Cue", scene.getId()};
  std::string errorMessage;

  REQUIRE(validateCueForScene(cue, scene, errorMessage));
  REQUIRE(errorMessage.empty());
}

