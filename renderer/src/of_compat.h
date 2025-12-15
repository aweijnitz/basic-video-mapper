#pragma once

// Compatibility layer that prefers a real openFrameworks installation when available,
// but falls back to the bundled stub otherwise.
#if defined(USE_OPENFRAMEWORKS)
#include <ofMain.h>

#if __has_include(<ofxMidi.h>)
#include <ofxMidi.h>
#else
// Minimal stand-ins so the build works without the addon present.
class ofxMidiListener {
 public:
  virtual ~ofxMidiListener() = default;
  virtual void newMidiMessage(class ofxMidiMessage& msg) = 0;
};

class ofxMidiMessage {
 public:
  enum Status { MIDI_CONTROL_CHANGE = 0xB0 };
  Status status{MIDI_CONTROL_CHANGE};
  int control{0};
  int value{0};
};

class ofxMidiIn {
 public:
  bool openPort(int /*portNumber*/) { return true; }
  void addListener(ofxMidiListener* /*listener*/) {}
  void closePort() {}
};
#endif

#if __has_include(<ofxFft.h>)
#include <ofxFft.h>
#else
#include <memory>
#include <vector>

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
#endif

#else  // USE_OPENFRAMEWORKS
#include "openFrameworksStub/ofMain.h"
#endif
