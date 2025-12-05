#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "projection/core/RendererProtocol.h"

using namespace projection::core;
using nlohmann::json;

TEST_CASE("RendererProtocol round trip Hello", "[RendererProtocol]") {
  RendererMessage message{RendererMessageType::Hello,
                          "cmd-hello",
                          HelloMessage{"0.1.0", "renderer"},
                          std::nullopt,
                          std::nullopt,
                          std::nullopt,
                          std::nullopt,
                          std::nullopt};

  json j = message;
  auto parsed = j.get<RendererMessage>();

  REQUIRE(parsed == message);
}

TEST_CASE("RendererProtocol round trip Ack and Error", "[RendererProtocol]") {
  RendererMessage ackMessage{RendererMessageType::Ack,
                             "cmd-ack",
                             std::nullopt,
                             AckMessage{"cmd-target"},
                             std::nullopt,
                             std::nullopt,
                             std::nullopt,
                             std::nullopt};

  json ackJson = ackMessage;
  auto parsedAck = ackJson.get<RendererMessage>();
  REQUIRE(parsedAck == ackMessage);

  RendererMessage errorMessage{RendererMessageType::Error,
                               "cmd-error",
                               std::nullopt,
                               std::nullopt,
                               ErrorMessage{"cmd-failed", "Something went wrong"},
                               std::nullopt,
                               std::nullopt,
                               std::nullopt};

  json errorJson = errorMessage;
  auto parsedError = errorJson.get<RendererMessage>();
  REQUIRE(parsedError == errorMessage);
}

TEST_CASE("RendererProtocol round trip scene and feed commands", "[RendererProtocol]") {
  RendererMessage loadSceneMessage{RendererMessageType::LoadScene,
                                   "cmd-load",
                                   std::nullopt,
                                   std::nullopt,
                                   std::nullopt,
                                   LoadSceneMessage{SceneId{"scene-123"}},
                                   std::nullopt,
                                   std::nullopt};

  json loadSceneJson = loadSceneMessage;
  auto parsedLoadScene = loadSceneJson.get<RendererMessage>();
  REQUIRE(parsedLoadScene == loadSceneMessage);

  RendererMessage setFeedMessage{RendererMessageType::SetFeedForSurface,
                                 "cmd-set-feed",
                                 std::nullopt,
                                 std::nullopt,
                                 std::nullopt,
                                 std::nullopt,
                                 SetFeedForSurfaceMessage{SurfaceId{"surface-1"}, FeedId{"feed-9"}},
                                 std::nullopt};

  json setFeedJson = setFeedMessage;
  auto parsedSetFeed = setFeedJson.get<RendererMessage>();
  REQUIRE(parsedSetFeed == setFeedMessage);

  RendererMessage playCueMessage{RendererMessageType::PlayCue,
                                 "cmd-play",
                                 std::nullopt,
                                 std::nullopt,
                                 std::nullopt,
                                 std::nullopt,
                                 std::nullopt,
                                 PlayCueMessage{CueId{"cue-5"}}};

  json playCueJson = playCueMessage;
  auto parsedPlayCue = playCueJson.get<RendererMessage>();
  REQUIRE(parsedPlayCue == playCueMessage);
}

TEST_CASE("RendererProtocol rejects invalid type", "[RendererProtocol]") {
  json invalidType = {{"type", "unknown"}, {"commandId", "cmd"}, {"payload", json::object()}};

  bool threw = false;
  try {
    invalidType.get<RendererMessage>();
  } catch (const std::runtime_error& ex) {
    threw = true;
    REQUIRE(std::string(ex.what()) == "Invalid RendererMessageType: unknown");
  }
  REQUIRE(threw);
}

TEST_CASE("RendererProtocol requires payload for typed messages", "[RendererProtocol]") {
  json missingPayload = {{"type", "hello"}, {"commandId", "cmd-no-payload"}};

  bool threw = false;
  try {
    missingPayload.get<RendererMessage>();
  } catch (const std::runtime_error& ex) {
    threw = true;
    REQUIRE(std::string(ex.what()) == "Missing required field: payload");
  }
  REQUIRE(threw);
}

TEST_CASE("RendererProtocol requires commandId", "[RendererProtocol]") {
  json missingCommandId = {{"type", "hello"}, {"payload", json{{"version", "0.1"}, {"role", "renderer"}}}};

  bool threw = false;
  try {
    missingCommandId.get<RendererMessage>();
  } catch (const std::runtime_error& ex) {
    threw = true;
    REQUIRE(std::string(ex.what()) == "Missing required field: commandId");
  }
  REQUIRE(threw);
}

