#pragma once

#include <iostream>
#include <string>

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
inline void ofSetColor(int /*r*/, int /*g*/, int /*b*/) {}
inline void ofDrawBitmapString(const std::string& /*text*/, float /*x*/, float /*y*/) {}
