#include "ofApp.h"

#include <ofMain.h>

#include <iostream>
#include <cstdlib>
#include <string>
#include <vector>
#include <unistd.h>

namespace {
struct Args {
  std::string host;
  int port;
  std::string name;
  bool verbose;
};

std::string defaultHost() {
  const char* envHost = std::getenv("RENDERER_HOST");
  if (envHost && *envHost) {
    return envHost;
  }
  return "127.0.0.1";
}

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

std::string defaultName() {
  const char* envName = std::getenv("RENDERER_NAME");
  if (envName && *envName) {
    return envName;
  }
  return "renderer-" + std::to_string(static_cast<long long>(::getpid()));
}

Args parseArgs(int argc, char* argv[]) {
  Args args{defaultHost(), defaultPort(), defaultName(), false};
  for (int i = 1; i < argc; ++i) {
    std::string arg(argv[i]);
    if (arg == "--server-host" && i + 1 < argc) {
      args.host = argv[++i];
    } else if (arg.rfind("--server-host=", 0) == 0) {
      args.host = arg.substr(14);
    } else if (arg == "--server-port" && i + 1 < argc) {
      args.port = std::stoi(argv[++i]);
    } else if (arg.rfind("--server-port=", 0) == 0) {
      args.port = std::stoi(arg.substr(14));
    } else if (arg == "--name" && i + 1 < argc) {
      args.name = argv[++i];
    } else if (arg.rfind("--name=", 0) == 0) {
      args.name = arg.substr(7);
    } else if (arg == "--port" && i + 1 < argc) {
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
  return ofRunApp(new ofApp(args.host, args.port, args.name, args.verbose));
}
