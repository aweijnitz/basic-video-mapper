#include "ofApp.h"

#include <cstdlib>
#include <iostream>
#include <utility>

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

void ofApp::update() {}

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
  std::lock_guard<std::mutex> lock(stateMutex_);
  lastError_.clear();

  switch (message.type) {
    case RendererMessageType::Hello:
      updateStatusForHello(*message.hello, message.commandId);
      break;
    case RendererMessageType::LoadScene:
      updateStatusForLoadScene(*message.loadScene, message.commandId);
      break;
    case RendererMessageType::SetFeedForSurface:
      updateStatusForSetFeed(*message.setFeedForSurface, message.commandId);
      break;
    case RendererMessageType::PlayCue:
      updateStatusForPlayCue(*message.playCue, message.commandId);
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

