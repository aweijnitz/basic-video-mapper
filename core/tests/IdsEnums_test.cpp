#include <catch2/catch_test_macros.hpp>

#include "projection/core/Enums.h"
#include "projection/core/Ids.h"

using namespace projection::core;

TEST_CASE("Identifier wrappers compare by value", "[ids]") {
  SceneId sceneA{"scene-1"};
  SceneId sceneB{"scene-1"};
  SceneId sceneC{"scene-2"};

  REQUIRE(sceneA == sceneB);
  REQUIRE(sceneA != sceneC);

  SurfaceId surface = makeSurfaceId("surface-1");
  REQUIRE(surface == SurfaceId{"surface-1"});

  FeedId feedOne{"feed-1"};
  FeedId feedTwo{"feed-2"};
  REQUIRE(feedOne != feedTwo);

  CueId cue = makeCueId("cue-1");
  REQUIRE(cue.value == std::string("cue-1"));
}

TEST_CASE("FeedType string conversion succeeds for known values", "[enums]") {
  REQUIRE(toString(FeedType::VideoFile) == "VideoFile");
  REQUIRE(toString(FeedType::Camera) == "Camera");
  REQUIRE(toString(FeedType::Generated) == "Generated");

  FeedType type{};
  REQUIRE(fromString("VideoFile", type));
  REQUIRE(type == FeedType::VideoFile);
  REQUIRE(fromString("Camera", type));
  REQUIRE(type == FeedType::Camera);
  REQUIRE(fromString("Generated", type));
  REQUIRE(type == FeedType::Generated);
}

TEST_CASE("BlendMode string conversion succeeds for known values", "[enums]") {
  REQUIRE(toString(BlendMode::Normal) == "Normal");
  REQUIRE(toString(BlendMode::Additive) == "Additive");
  REQUIRE(toString(BlendMode::Multiply) == "Multiply");

  BlendMode mode{};
  REQUIRE(fromString("Normal", mode));
  REQUIRE(mode == BlendMode::Normal);
  REQUIRE(fromString("Additive", mode));
  REQUIRE(mode == BlendMode::Additive);
  REQUIRE(fromString("Multiply", mode));
  REQUIRE(mode == BlendMode::Multiply);
}

TEST_CASE("Enum parsing fails gracefully for invalid strings", "[enums]") {
  FeedType feedType = FeedType::VideoFile;
  BlendMode blendMode = BlendMode::Normal;

  REQUIRE(!fromString("", feedType));
  REQUIRE(!fromString("unknown", feedType));
  REQUIRE(feedType == FeedType::VideoFile);

  REQUIRE(!fromString("Invalid", blendMode));
  REQUIRE(!fromString("123", blendMode));
  REQUIRE(blendMode == BlendMode::Normal);
}
