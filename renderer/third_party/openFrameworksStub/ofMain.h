#pragma once

#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

// Minimal stub of openFrameworks headers for building without the real library.
// Provides the interfaces used by this skeleton renderer.

static constexpr int OF_WINDOW = 0;

enum ofLoopType { OF_LOOP_NONE = 0, OF_LOOP_NORMAL };

class ofVideoPlayer {
 public:
  bool load(const std::string& filePath) {
    loadedPath_ = filePath;
    return true;
  }

  void setLoopState(ofLoopType /*type*/) {}
  void play() { playing_ = true; }
  void update() {}

  void draw(float /*x*/, float /*y*/, float /*w*/, float /*h*/) const {}

  const std::string& loadedPath() const { return loadedPath_; }
  bool isPlaying() const { return playing_; }

 private:
  std::string loadedPath_{};
  bool playing_{false};
};

class ofBaseApp {
 public:
  virtual ~ofBaseApp() = default;
  virtual void setup() {}
  virtual void update() {}
  virtual void draw() {}
  virtual void exit() {}
};

class ofBaseSoundInput {
 public:
  virtual ~ofBaseSoundInput() = default;
  virtual void audioIn(class ofSoundBuffer& buffer) = 0;
};

class ofSoundBuffer {
 public:
  ofSoundBuffer() = default;
  ofSoundBuffer(const std::vector<float>& buffer, int numChannels)
      : buffer_(buffer), numChannels_(numChannels) {}

  float& operator[](size_t index) { return buffer_[index]; }
  const float& operator[](size_t index) const { return buffer_[index]; }

  size_t size() const { return buffer_.size(); }
  int getNumChannels() const { return numChannels_; }
  int getNumFrames() const { return numChannels_ > 0 ? static_cast<int>(buffer_.size() / numChannels_) : 0; }

  float* data() { return buffer_.data(); }
  const float* data() const { return buffer_.data(); }

  std::vector<float>& getBuffer() { return buffer_; }
  const std::vector<float>& getBuffer() const { return buffer_; }

 private:
  std::vector<float> buffer_{};
  int numChannels_{1};
};

class ofSoundStream {
 public:
  void setup(ofBaseSoundInput* input, int /*outChannels*/, int inChannels, int /*sampleRate*/, int bufferSize,
             int /*nBuffers*/ = 4) {
    input_ = input;
    buffer_.getBuffer().assign(static_cast<size_t>(bufferSize * std::max(inChannels, 1)), 0.0f);
    bufferChannels_ = inChannels;
  }

  void start() {}
  void stop() {}

  void simulateAudioInput(const std::vector<float>& samples) {
    buffer_.getBuffer() = samples;
    if (bufferChannels_ > 0) {
      buffer_.getBuffer().resize(static_cast<size_t>(buffer_.getNumFrames() * bufferChannels_));
    }
    if (input_) {
      buffer_ = ofSoundBuffer(samples, bufferChannels_ == 0 ? 1 : bufferChannels_);
      input_->audioIn(buffer_);
    }
  }

 private:
  ofBaseSoundInput* input_{nullptr};
  ofSoundBuffer buffer_{};
  int bufferChannels_{1};
};

struct ofxMidiMessage {
  enum Status { MIDI_CONTROL_CHANGE = 0xB0 };

  Status status{MIDI_CONTROL_CHANGE};
  int control{0};
  int value{0};
};

class ofxMidiListener {
 public:
  virtual ~ofxMidiListener() = default;
  virtual void newMidiMessage(ofxMidiMessage& msg) = 0;
};

class ofxMidiIn {
 public:
  bool openPort(int /*portNumber*/) { return true; }
  void addListener(ofxMidiListener* listener) { listener_ = listener; }
  void closePort() {}

  void simulateMidiMessage(const ofxMidiMessage& msg) {
    if (listener_) {
      ofxMidiMessage copy = msg;
      listener_->newMidiMessage(copy);
    }
  }

 private:
  ofxMidiListener* listener_{nullptr};
};

class ofxFft {
 public:
  static std::unique_ptr<ofxFft> create(int size) { return std::unique_ptr<ofxFft>(new ofxFft(size)); }

  explicit ofxFft(int size) : size_(size), magnitude_(static_cast<size_t>(size / 2), 0.0f) {}

  void setSignal(const float* signal) {
    if (!signal) {
      std::fill(magnitude_.begin(), magnitude_.end(), 0.0f);
      return;
    }
    for (size_t i = 0; i < magnitude_.size(); ++i) {
      magnitude_[i] = std::abs(signal[i]);
    }
  }

  const float* getMagnitude() const { return magnitude_.data(); }
  int getBinSize() const { return static_cast<int>(magnitude_.size()); }

 private:
  int size_{0};
  std::vector<float> magnitude_{};
};

inline void ofSetupOpenGL(int /*w*/, int /*h*/, int /*screenMode*/) {}

inline int ofRunApp(ofBaseApp* app) {
  if (app) {
    app->setup();
    // Run a tiny loop to satisfy the interface; real rendering would have an event loop.
    app->update();
    app->draw();
    app->exit();
  }
  delete app;
  return 0;
}

inline void ofBackground(int /*r*/, int /*g*/, int /*b*/) {}
inline void ofSetColor(int /*r*/, int /*g*/, int /*b*/, int /*a*/) {}
inline void ofSetColor(int /*r*/, int /*g*/, int /*b*/) {}
inline void ofDrawBitmapString(const std::string& /*text*/, float /*x*/, float /*y*/) {}
inline void ofPushMatrix() {}
inline void ofPopMatrix() {}
inline void ofTranslate(float /*x*/, float /*y*/) {}
inline void ofScale(float /*x*/, float /*y*/) {}
inline float ofGetWidth() { return 1024.0f; }
inline float ofGetHeight() { return 768.0f; }
