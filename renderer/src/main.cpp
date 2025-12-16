#include "ofApp.h"

#include <ofMain.h>

#include <iostream>
#include <cstdlib>
#include <string>
#include <vector>

namespace {
struct Args {
  int port;
  bool verbose;
};

int defaultPort() {
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

Args parseArgs(int argc, char* argv[]) {
  Args args{defaultPort(), false};
  for (int i = 1; i < argc; ++i) {
    std::string arg(argv[i]);
    if (arg == "--port" && i + 1 < argc) {
      args.port = std::stoi(argv[++i]);
    } else if (arg.rfind("--port=", 0) == 0) {
      args.port = std::stoi(arg.substr(7));
    } else if (arg == "--verbose") {
      args.verbose = true;
    }
  }
  return args;
}
}  // namespace

int main(int argc, char* argv[]) {
  const auto args = parseArgs(argc, argv);
  if (args.verbose) {
    std::cerr << "[renderer] verbose mode on" << std::endl;
  }
  ofSetupOpenGL(640, 480, OF_WINDOW);
  return ofRunApp(new ofApp(args.port, args.verbose));
}
