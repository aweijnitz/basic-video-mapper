#pragma once

#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

#include "RenderState.h"
#include "net/RendererServer.h"
#include "openFrameworksStub/ofMain.h"
#include "util/InteractionUtils.h"

class ofApp : public ofBaseApp,
              public ofBaseSoundInput,
              public ofxMidiListener,
              public projection::renderer::RendererCommandHandler {
 public:
  ofApp();

  void setup() override;
  void update() override;
  void draw() override;
  void exit() override;

  void audioIn(ofSoundBuffer& input) override;
  void newMidiMessage(ofxMidiMessage& msg) override;

  void handle(const projection::core::RendererMessage& message) override;

 private:
  void updateStatusForHello(const projection::core::HelloMessage& hello, const std::string& commandId);
  void updateStatusForLoadScene(const projection::core::LoadSceneMessage& loadScene,
                                const std::string& commandId);
  void updateStatusForSetFeed(const projection::core::SetFeedForSurfaceMessage& setFeed,
                              const std::string& commandId);
  void updateStatusForPlayCue(const projection::core::PlayCueMessage& playCue, const std::string& commandId);
  void processMessage(const projection::core::RendererMessage& message);

  projection::renderer::RendererServer server_;
  int port_;

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

  ofxMidiIn midiIn_{};
  float midiBrightness_{1.0f};

  ofSoundStream soundStream_{};
  std::vector<float> audioBuffer_{};
  std::unique_ptr<ofxFft> fft_{};
  float audioScale_{1.0f};
  float smoothedEnergy_{0.0f};
};

