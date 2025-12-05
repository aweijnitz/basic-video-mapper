#pragma once

#include <iostream>
#include <string>

// Minimal stub of openFrameworks headers for building without the real library.
// Provides the interfaces used by this skeleton renderer.

static constexpr int OF_WINDOW = 0;

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
