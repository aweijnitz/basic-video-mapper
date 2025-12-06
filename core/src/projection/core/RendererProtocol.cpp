#include "projection/core/RendererProtocol.h"

#include <stdexcept>
#include <string>
#include <unordered_map>

#include "projection/core/Serialization.h"

using nlohmann::json;

namespace projection::core {
namespace {

const std::unordered_map<RendererMessageType, std::string> kRendererMessageTypeToString{{RendererMessageType::Hello, "hello"},
                                                                                       {RendererMessageType::Ack, "ack"},
                                                                                       {RendererMessageType::Error, "error"},
                                                                                       {RendererMessageType::LoadScene, "loadScene"},
                                                                                       {RendererMessageType::LoadSceneDefinition, "loadSceneDefinition"},
                                                                                       {RendererMessageType::SetFeedForSurface, "setFeedForSurface"},
                                                                                       {RendererMessageType::PlayCue, "playCue"}};

std::string toString(RendererMessageType type) { return kRendererMessageTypeToString.at(type); }

RendererMessageType parseRendererMessageType(const std::string& raw) {
  for (const auto& [type, stringValue] : kRendererMessageTypeToString) {
    if (stringValue == raw) {
      return type;
    }
  }
  throw std::runtime_error("Invalid RendererMessageType: " + raw);
}

const json& requireField(const json& j, const std::string& key) {
  if (!j.contains(key)) {
    throw std::runtime_error("Missing required field: " + key);
  }
  return j.at(key);
}

std::string requireString(const json& j, const std::string& key) {
  const auto& field = requireField(j, key);
  if (!field.is_string()) {
    throw std::runtime_error("Field '" + key + "' must be a string");
  }
  return field.get<std::string>();
}

}  // namespace

void to_json(json& j, const RendererMessageType& type) { j = toString(type); }

void from_json(const json& j, RendererMessageType& type) {
  if (!j.is_string()) {
    throw std::runtime_error("RendererMessageType must be a string");
  }
  type = parseRendererMessageType(j.get<std::string>());
}

void to_json(json& j, const HelloMessage& message) {
  j = json{{"version", message.version}, {"role", message.role}};
}

void from_json(const json& j, HelloMessage& message) {
  if (!j.is_object()) {
    throw std::runtime_error("Hello payload must be an object");
  }
  message.version = requireString(j, "version");
  message.role = requireString(j, "role");
}

void to_json(json& j, const AckMessage& message) { j = json{{"commandId", message.commandId}}; }

void from_json(const json& j, AckMessage& message) {
  if (!j.is_object()) {
    throw std::runtime_error("Ack payload must be an object");
  }
  message.commandId = requireString(j, "commandId");
}

void to_json(json& j, const ErrorMessage& message) {
  j = json{{"commandId", message.commandId}, {"message", message.message}};
}

void from_json(const json& j, ErrorMessage& message) {
  if (!j.is_object()) {
    throw std::runtime_error("Error payload must be an object");
  }
  message.commandId = requireString(j, "commandId");
  message.message = requireString(j, "message");
}

void to_json(json& j, const LoadSceneMessage& message) { j = json{{"sceneId", message.sceneId.value}}; }

void from_json(const json& j, LoadSceneMessage& message) {
  if (!j.is_object()) {
    throw std::runtime_error("LoadScene payload must be an object");
  }
  message.sceneId = SceneId(requireString(j, "sceneId"));
}

void to_json(json& j, const LoadSceneDefinitionMessage& message) {
  j = json{{"scene", message.scene}, {"feeds", message.feeds}};
}

void from_json(const json& j, LoadSceneDefinitionMessage& message) {
  if (!j.is_object()) {
    throw std::runtime_error("LoadSceneDefinition payload must be an object");
  }

  if (!j.contains("scene")) {
    throw std::runtime_error("Missing required field: scene");
  }
  if (!j.at("scene").is_object()) {
    throw std::runtime_error("Field 'scene' must be an object");
  }
  message.scene = j.at("scene").get<Scene>();

  if (!j.contains("feeds")) {
    throw std::runtime_error("Missing required field: feeds");
  }
  const auto& feedsJson = j.at("feeds");
  if (!feedsJson.is_array()) {
    throw std::runtime_error("Field 'feeds' must be an array");
  }
  message.feeds = feedsJson.get<std::vector<Feed>>();
}

void to_json(json& j, const SetFeedForSurfaceMessage& message) {
  j = json{{"surfaceId", message.surfaceId.value}, {"feedId", message.feedId.value}};
}

void from_json(const json& j, SetFeedForSurfaceMessage& message) {
  if (!j.is_object()) {
    throw std::runtime_error("SetFeedForSurface payload must be an object");
  }
  message.surfaceId = SurfaceId(requireString(j, "surfaceId"));
  message.feedId = FeedId(requireString(j, "feedId"));
}

void to_json(json& j, const PlayCueMessage& message) { j = json{{"cueId", message.cueId.value}}; }

void from_json(const json& j, PlayCueMessage& message) {
  if (!j.is_object()) {
    throw std::runtime_error("PlayCue payload must be an object");
  }
  message.cueId = CueId(requireString(j, "cueId"));
}

void to_json(json& j, const RendererMessage& message) {
  j = json{{"type", message.type}, {"commandId", message.commandId}};

  json payload;
  switch (message.type) {
    case RendererMessageType::Hello:
      if (!message.hello) {
        throw std::runtime_error("Hello message missing payload");
      }
      payload = *message.hello;
      break;
    case RendererMessageType::Ack:
      if (!message.ack) {
        throw std::runtime_error("Ack message missing payload");
      }
      payload = *message.ack;
      break;
    case RendererMessageType::Error:
      if (!message.error) {
        throw std::runtime_error("Error message missing payload");
      }
      payload = *message.error;
      break;
    case RendererMessageType::LoadScene:
      if (!message.loadScene) {
        throw std::runtime_error("LoadScene message missing payload");
      }
      payload = *message.loadScene;
      break;
    case RendererMessageType::LoadSceneDefinition:
      if (!message.loadSceneDefinition) {
        throw std::runtime_error("LoadSceneDefinition message missing payload");
      }
      payload = *message.loadSceneDefinition;
      break;
    case RendererMessageType::SetFeedForSurface:
      if (!message.setFeedForSurface) {
        throw std::runtime_error("SetFeedForSurface message missing payload");
      }
      payload = *message.setFeedForSurface;
      break;
    case RendererMessageType::PlayCue:
      if (!message.playCue) {
        throw std::runtime_error("PlayCue message missing payload");
      }
      payload = *message.playCue;
      break;
  }

  if (!payload.is_null()) {
    j["payload"] = payload;
  }
}

void from_json(const json& j, RendererMessage& message) {
  if (!j.is_object()) {
    throw std::runtime_error("RendererMessage must be an object");
  }

  message.type = parseRendererMessageType(requireString(j, "type"));
  message.commandId = requireString(j, "commandId");

  const auto payloadIt = j.find("payload");
  if (payloadIt == j.end()) {
    throw std::runtime_error("Missing required field: payload");
  }
  const auto& payload = *payloadIt;
  if (!payload.is_object()) {
    throw std::runtime_error("Field 'payload' must be an object");
  }

  switch (message.type) {
    case RendererMessageType::Hello: {
      HelloMessage helloMessage;
      from_json(payload, helloMessage);
      message.hello = helloMessage;
      break;
    }
    case RendererMessageType::Ack: {
      AckMessage ackMessage;
      from_json(payload, ackMessage);
      message.ack = ackMessage;
      break;
    }
    case RendererMessageType::Error: {
      ErrorMessage errorMessage;
      from_json(payload, errorMessage);
      message.error = errorMessage;
      break;
    }
    case RendererMessageType::LoadScene: {
      LoadSceneMessage loadSceneMessage;
      from_json(payload, loadSceneMessage);
      message.loadScene = loadSceneMessage;
      break;
    }
    case RendererMessageType::LoadSceneDefinition: {
      LoadSceneDefinitionMessage loadSceneDefinitionMessage;
      from_json(payload, loadSceneDefinitionMessage);
      message.loadSceneDefinition = loadSceneDefinitionMessage;
      break;
    }
    case RendererMessageType::SetFeedForSurface: {
      SetFeedForSurfaceMessage setFeedMessage;
      from_json(payload, setFeedMessage);
      message.setFeedForSurface = setFeedMessage;
      break;
    }
    case RendererMessageType::PlayCue: {
      PlayCueMessage playCueMessage;
      from_json(payload, playCueMessage);
      message.playCue = playCueMessage;
      break;
    }
  }
}

}  // namespace projection::core

