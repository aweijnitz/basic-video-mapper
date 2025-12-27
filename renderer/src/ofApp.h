#pragma once

#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

#include <ofMain.h>
#if __has_include(<ofxMidi.h>)
#include <ofxMidi.h>
#define PROJECTION_HAS_OFX_MIDI 1
#else
#define PROJECTION_HAS_OFX_MIDI 0
#endif

#include "RenderState.h"
#include "net/RendererClient.h"
#include "util/InteractionUtils.h"

class ofApp : public ofBaseApp,
#if PROJECTION_HAS_OFX_MIDI
              public ofxMidiListener,
#endif
              public projection::renderer::RendererCommandHandler {
 public:
  explicit ofApp(std::string host, int port, std::string name, bool verbose = false);

  void setup() override;
  void update() override;
  void draw() override;
  void exit() override;

#if PROJECTION_HAS_OFX_MIDI
  void audioIn(ofSoundBuffer& input) override;
  void newMidiMessage(ofxMidiMessage& msg) override;
#else
  void audioIn(ofSoundBuffer& input) override;
#endif

  void handle(const projection::core::RendererMessage& message) override;

 private:
  void updateStatusForHello(const projection::core::HelloMessage& hello, const std::string& commandId);
  void updateStatusForLoadScene(const projection::core::LoadSceneMessage& loadScene,
                                const std::string& commandId);
  void updateStatusForSetFeed(const projection::core::SetFeedForSurfaceMessage& setFeed,
                              const std::string& commandId);
  void updateStatusForPlayCue(const projection::core::PlayCueMessage& playCue, const std::string& commandId);
  void processMessage(const projection::core::RendererMessage& message);

  projection::renderer::RendererClient client_;
  std::string host_;
  int port_;
  std::string name_;
  bool verbose_{false};

  projection::renderer::RenderState renderState_{};

  std::mutex queueMutex_{};
  std::queue<projection::core::RendererMessage> messageQueue_{};
  std::mutex stateMutex_{};
  std::mutex audioMutex_{};
  std::string lastCommand_;
  std::string lastError_;
  std::string sceneId_;
  std::string rendererRole_;
  std::string rendererVersion_;

#if PROJECTION_HAS_OFX_MIDI
  ofxMidiIn midiIn_{};
#endif
  float midiBrightness_{1.0f};

  ofSoundStream soundStream_{};
  std::vector<float> audioBuffer_{};
  float audioScale_{1.0f};
  float smoothedEnergy_{0.0f};
};
