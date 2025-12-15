#include "ofApp.h"

#include <algorithm>
#include <cmath>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <utility>
#include <vector>

using projection::core::RendererMessageType;

ofApp::ofApp(int port, bool verbose) : server_(*this, verbose), port_(port), verbose_(verbose) {}

void ofApp::setup() {
  if (verbose_) {
    std::cerr << "[renderer] starting on port " << port_ << std::endl;
  }
  server_.start(port_);

  midiIn_.openPort(0);
  midiIn_.addListener(this);

  const int inputChannels = 1;
  const int outputChannels = 0;
  const int sampleRate = 44100;
  const int bufferSize = 512;
  soundStream_.setup(this, outputChannels, inputChannels, sampleRate, bufferSize, 4);
  fft_ = ofxFft::create(bufferSize);
  if (verbose_) {
    std::cerr << "[renderer] audio/midi initialized" << std::endl;
  }
}

void ofApp::update() {
  if (!server_.running()) {
    const std::string serverError = server_.lastError();
    if (!serverError.empty()) {
      std::lock_guard<std::mutex> lock(stateMutex_);
      lastError_ = serverError;
    }
    std::raise(SIGTERM);
    return;
  }

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

  std::vector<float> audioCopy;
  {
    std::lock_guard<std::mutex> lock(audioMutex_);
    audioCopy = audioBuffer_;
  }

  if (fft_ && !audioCopy.empty()) {
    fft_->setSignal(audioCopy.data());
    const int binSize = fft_->getBinSize();
    std::vector<float> magnitudes(fft_->getMagnitude(), fft_->getMagnitude() + binSize);
    const float averageEnergy = projection::renderer::computeAverageEnergy(magnitudes, 32);

    constexpr float smoothingFactor = 0.9f;
    smoothedEnergy_ = smoothingFactor * smoothedEnergy_ + (1.0f - smoothingFactor) * averageEnergy;
    audioScale_ = projection::renderer::mapEnergyToScale(smoothedEnergy_);
  }
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

  // Draw loaded video feeds onto their skewed surfaces using textured meshes.
  const auto& scene = renderState_.currentScene();
  const auto& videoFeeds = renderState_.videoFeeds();

  const float screenW = static_cast<float>(ofGetWidth());
  const float screenH = static_cast<float>(ofGetHeight());

  ofPushMatrix();
  ofTranslate(screenW / 2.0f, screenH / 2.0f);
  ofScale(audioScale_, audioScale_);
  ofTranslate(-screenW / 2.0f, -screenH / 2.0f);

  for (const auto& surface : scene.getSurfaces()) {
    auto feedIt = videoFeeds.find(surface.getFeedId().value);
    if (feedIt == videoFeeds.end()) {
      continue;
    }

    auto& player = feedIt->second.player;
    if (!player.isLoaded() || player.getTexture().getTextureData().textureID == 0) {
      continue;
    }

    const auto& vertices = surface.getVertices();
    if (vertices.size() < 3) {
      continue;
    }

    std::vector<glm::vec2> screenVerts;
    screenVerts.reserve(vertices.size());
    for (const auto& v : vertices) {
      // Scene coordinates are normalized -1..1; map to screen pixels.
      float x = (v.x * 0.5f + 0.5f) * screenW;
      float y = (v.y * 0.5f + 0.5f) * screenH;
      screenVerts.emplace_back(x, y);
    }

    float minX = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float minY = std::numeric_limits<float>::max();
    float maxY = std::numeric_limits<float>::lowest();
    for (const auto& v : screenVerts) {
      minX = std::min(minX, v.x);
      maxX = std::max(maxX, v.x);
      minY = std::min(minY, v.y);
      maxY = std::max(maxY, v.y);
    }
    const float videoW = player.getWidth();
    const float videoH = player.getHeight();
    if (videoW <= 0.0f || videoH <= 0.0f || maxX <= minX || maxY <= minY) {
      continue;
    }

    ofMesh mesh;
    mesh.setMode(OF_PRIMITIVE_TRIANGLE_FAN);
    for (const auto& v : screenVerts) {
      float u = ofMap(v.x, minX, maxX, 0.0f, videoW, true);
      float t = ofMap(v.y, minY, maxY, 0.0f, videoH, true);
      mesh.addVertex(glm::vec3(v.x, v.y, 0.0f));
      mesh.addTexCoord(glm::vec2(u, t));
    }

    const float alpha = std::clamp(surface.getOpacity() * midiBrightness_, 0.0f, 1.0f);
    const float brightness = std::clamp(surface.getBrightness(), 0.0f, 1.0f);
    const int alphaValue = static_cast<int>(std::round(alpha * 255.0f));
    const int colorValue = static_cast<int>(std::round(brightness * 255.0f));
    ofSetColor(colorValue, colorValue, colorValue, alphaValue);

    auto& texture = player.getTexture();
    if (texture.isAllocated()) {
      texture.bind();
      mesh.draw();
      texture.unbind();
    }
  }

  ofPopMatrix();
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

void ofApp::audioIn(ofSoundBuffer& input) {
  const int channels = std::max<int>(1, input.getNumChannels());
  std::vector<float> mono;
  mono.reserve(static_cast<size_t>(input.getNumFrames()));
  for (int i = 0; i < input.getNumFrames(); ++i) {
    mono.push_back(input[static_cast<size_t>(i * channels)]);
  }

  std::lock_guard<std::mutex> lock(audioMutex_);
  audioBuffer_ = std::move(mono);
}

void ofApp::newMidiMessage(ofxMidiMessage& msg) {
  if (msg.status == ofxMidiMessage::MIDI_CONTROL_CHANGE && msg.control == 1) {
    midiBrightness_ = projection::renderer::mapMidiValueToBrightness(msg.value);
  }
}

void ofApp::exit() {
  soundStream_.stop();
  midiIn_.closePort();
  server_.stop();
}

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
      if (verbose_) {
        std::cerr << "[renderer] LoadSceneDefinition with scene " << message.loadSceneDefinition->scene.getId().value
                  << " feeds=" << message.loadSceneDefinition->feeds.size() << std::endl;
      }
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
