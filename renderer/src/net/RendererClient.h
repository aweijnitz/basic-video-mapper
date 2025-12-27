#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

#include <projection/core/RendererProtocol.h>

#include "net/RendererServer.h"

namespace projection::renderer {

class RendererClient {
 public:
  RendererClient(RendererCommandHandler& handler,
                 std::string host,
                 int port,
                 std::string name,
                 bool verbose = false);
  ~RendererClient();

  RendererClient(const RendererClient&) = delete;
  RendererClient& operator=(const RendererClient&) = delete;

  void start();
  void stop();

  bool running() const { return running_; }
  std::string lastError() const;

  const std::string& host() const { return host_; }
  int port() const { return port_; }
  const std::string& name() const { return name_; }

 private:
  void run();
  void readLoop(std::string buffer);
  void processLine(const std::string& line);
  void sendMessage(const projection::core::RendererMessage& message);
  void sendAck(const std::string& commandId);
  void sendError(const std::string& commandId, const std::string& errorText);
  std::string generateCommandId() const;

  RendererCommandHandler& handler_;
  std::string host_;
  int port_;
  std::string name_;
  std::atomic<bool> running_{false};
  bool verbose_{false};
  int socketFd_{-1};
  std::thread clientThread_{};
  mutable std::mutex errorMutex_{};
  std::string lastError_{};
  std::mutex socketMutex_{};
};

}  // namespace projection::renderer
