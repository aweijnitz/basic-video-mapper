#include <catch2/catch_test_macros.hpp>

#include "RenderState.h"

#include <exception>
#include <vector>

#include <projection/core/Feed.h>
#include <projection/core/Scene.h>
#include <projection/core/Surface.h>

using projection::core::Feed;
using projection::core::FeedId;
using projection::core::FeedType;
using projection::core::Scene;
using projection::core::SceneId;
using projection::core::Surface;
using projection::core::SurfaceId;
using projection::core::Vec2;
using projection::renderer::RenderState;
using projection::renderer::mapVideoFeedFilePaths;

TEST_CASE("mapVideoFeedFilePaths returns mappings for video feeds", "[renderer][renderstate]") {
  Scene scene{SceneId{"scene-1"}, "Test Scene", "desc", {}};
  std::vector<Feed> feeds{
      projection::core::makeVideoFileFeed(FeedId{"video1"}, "Video 1", "/media/video1.mp4"),
      Feed{FeedId{"generated"}, "Generated", FeedType::Generated, "{}"}};

  auto mapping = mapVideoFeedFilePaths(scene, feeds);

  REQUIRE(mapping.size() == 1);
  REQUIRE(mapping.at("video1") == "/media/video1.mp4");
  REQUIRE(mapping.find("generated") == mapping.end());
}

TEST_CASE("mapVideoFeedFilePaths throws when config is invalid", "[renderer][renderstate][error]") {
  Scene scene{SceneId{"scene-2"}, "Scene", "desc", {}};
  std::vector<Feed> feeds{
      Feed{FeedId{"video2"}, "Video 2", FeedType::VideoFile, "{}"}};

  bool threw = false;
  try {
    mapVideoFeedFilePaths(scene, feeds);
  } catch (const std::exception&) {
    threw = true;
  }
  REQUIRE(threw);
}

TEST_CASE("loadSceneDefinition stores scene, feeds, and video resources", "[renderer][renderstate]") {
  Surface surface{SurfaceId{"surface-1"}, "Surface", {Vec2{0, 0}, Vec2{100, 0}, Vec2{100, 100}, Vec2{0, 100}},
                  FeedId{"video1"}};
  Scene scene{SceneId{"scene-3"}, "Scene 3", "desc", {surface}};
  std::vector<Feed> feeds{
      projection::core::makeVideoFileFeed(FeedId{"video1"}, "Video 1", "/media/video1.mp4"),
      Feed{FeedId{"camera1"}, "Camera", FeedType::Camera, "{}"}};

  RenderState state;
  state.loadSceneDefinition(scene, feeds);

  REQUIRE(state.currentScene().getId().value == scene.getId().value);
  REQUIRE(state.currentFeeds().size() == feeds.size());

  const auto& videoFeeds = state.videoFeeds();
  REQUIRE(videoFeeds.size() == 1);
  auto it = videoFeeds.find("video1");
  REQUIRE(it != videoFeeds.end());
  REQUIRE(it->second.player.loadedPath() == "/media/video1.mp4");
  REQUIRE(it->second.player.isPlaying());
}

