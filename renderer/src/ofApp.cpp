#include "ofApp.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <utility>
#include <vector>

using projection::core::RendererMessageType;

namespace {
int resolvePort() {
  const char* envPort = std::getenv("RENDERER_PORT");
  if (envPort) {
    try {
      return std::stoi(envPort);
    } catch (...) {
      std::cerr << "Invalid RENDERER_PORT value, defaulting to 5050" << std::endl;
    }
  }
  return 5050;
}
}

ofApp::ofApp() : server_(*this), port_(resolvePort()) {}

void ofApp::setup() { server_.start(port_); }

void ofApp::update() {
  std::vector<projection::core::RendererMessage> messages;
  {
    std::lock_guard<std::mutex> lock(queueMutex_);
    while (!messageQueue_.empty()) {
      messages.push_back(messageQueue_.front());
      messageQueue_.pop();
    }
  }

  for (const auto& message : messages) {
    processMessage(message);
  }

  renderState_.updateVideoPlayers();
}

void ofApp::draw() {
  std::string lastCommand;
  std::string lastError;
  std::string sceneId;
  std::string role;
  std::string version;
  int serverPort = server_.port();

  {
    std::lock_guard<std::mutex> lock(stateMutex_);
    lastCommand = lastCommand_;
    lastError = lastError_;
    sceneId = sceneId_;
    role = rendererRole_;
    version = rendererVersion_;
  }

  ofBackground(0, 0, 0);
  ofSetColor(255, 255, 255);

  // Draw loaded video feeds on their surfaces.
  const auto& scene = renderState_.currentScene();
  const auto& videoFeeds = renderState_.videoFeeds();
  for (const auto& surface : scene.getSurfaces()) {
    auto it = videoFeeds.find(surface.getFeedId().value);
    if (it == videoFeeds.end()) {
      continue;
    }

    const auto& vertices = surface.getVertices();
    if (vertices.empty()) {
      continue;
    }

    float minX = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float minY = std::numeric_limits<float>::max();
    float maxY = std::numeric_limits<float>::lowest();
    for (const auto& vertex : vertices) {
      minX = std::min(minX, vertex.x);
      maxX = std::max(maxX, vertex.x);
      minY = std::min(minY, vertex.y);
      maxY = std::max(maxY, vertex.y);
    }

    float width = std::max(0.0f, maxX - minX);
    float height = std::max(0.0f, maxY - minY);
    it->second.player.draw(minX, minY, width, height);
  }

  ofDrawBitmapString("Renderer listening on port: " + std::to_string(serverPort), 20, 20);
  if (!role.empty()) {
    ofDrawBitmapString("Role: " + role + " | Version: " + version, 20, 40);
  }
  if (!sceneId.empty()) {
    ofDrawBitmapString("Loaded Scene: " + sceneId, 20, 60);
  }
  if (!lastCommand.empty()) {
    ofDrawBitmapString("Last Command: " + lastCommand, 20, 80);
  }
  if (!lastError.empty()) {
    ofSetColor(255, 0, 0);
    ofDrawBitmapString("Last Error: " + lastError, 20, 100);
  }
}

void ofApp::exit() { server_.stop(); }

void ofApp::handle(const projection::core::RendererMessage& message) {
  std::lock_guard<std::mutex> lock(queueMutex_);
  messageQueue_.push(message);
}

void ofApp::processMessage(const projection::core::RendererMessage& message) {
  {
    std::lock_guard<std::mutex> lock(stateMutex_);
    lastError_.clear();
  }

  switch (message.type) {
    case RendererMessageType::Hello:
      {
        std::lock_guard<std::mutex> lock(stateMutex_);
        updateStatusForHello(*message.hello, message.commandId);
      }
      break;
    case RendererMessageType::LoadScene:
      {
        std::lock_guard<std::mutex> lock(stateMutex_);
        updateStatusForLoadScene(*message.loadScene, message.commandId);
      }
      break;
    case RendererMessageType::LoadSceneDefinition:
      renderState_.loadSceneDefinition(message.loadSceneDefinition->scene, message.loadSceneDefinition->feeds);
      {
        std::lock_guard<std::mutex> lock(stateMutex_);
        sceneId_ = message.loadSceneDefinition->scene.getId().value;
        lastCommand_ = "LoadSceneDefinition (#" + message.commandId + ")";
      }
      break;
    case RendererMessageType::SetFeedForSurface:
      {
        std::lock_guard<std::mutex> lock(stateMutex_);
        updateStatusForSetFeed(*message.setFeedForSurface, message.commandId);
      }
      break;
    case RendererMessageType::PlayCue:
      {
        std::lock_guard<std::mutex> lock(stateMutex_);
        updateStatusForPlayCue(*message.playCue, message.commandId);
      }
      break;
    case RendererMessageType::Ack:
    case RendererMessageType::Error:
      // Renderer should not receive these in normal operation, ignore.
      break;
  }
}

void ofApp::updateStatusForHello(const projection::core::HelloMessage& hello, const std::string& commandId) {
  rendererRole_ = hello.role;
  rendererVersion_ = hello.version;
  lastCommand_ = "Hello (#" + commandId + ")";
}

void ofApp::updateStatusForLoadScene(const projection::core::LoadSceneMessage& loadScene,
                                     const std::string& commandId) {
  sceneId_ = loadScene.sceneId.value;
  lastCommand_ = "LoadScene (#" + commandId + ")";
}

void ofApp::updateStatusForSetFeed(const projection::core::SetFeedForSurfaceMessage& setFeed,
                                   const std::string& commandId) {
  lastCommand_ = "SetFeedForSurface (#" + commandId + ") -> surface " + setFeed.surfaceId.value +
                 " feed " + setFeed.feedId.value;
}

void ofApp::updateStatusForPlayCue(const projection::core::PlayCueMessage& playCue, const std::string& commandId) {
  lastCommand_ = "PlayCue (#" + commandId + ") -> cue " + playCue.cueId.value;
}

