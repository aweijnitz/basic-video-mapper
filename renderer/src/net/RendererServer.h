#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

#include <projection/core/RendererProtocol.h>

namespace projection::renderer {

projection::core::RendererMessage parseRendererMessageLine(const std::string& line);
std::string renderRendererMessageLine(const projection::core::RendererMessage& message);
projection::core::RendererMessage makeAckMessage(const std::string& commandId);
projection::core::RendererMessage makeErrorMessage(const std::string& commandId,
                                                   const std::string& errorText);

class RendererCommandHandler {
 public:
  virtual ~RendererCommandHandler() = default;
  virtual void handle(const projection::core::RendererMessage& message) = 0;
};

class RendererServer {
 public:
  explicit RendererServer(RendererCommandHandler& handler);
  ~RendererServer();

  RendererServer(const RendererServer&) = delete;
  RendererServer& operator=(const RendererServer&) = delete;

  void start(int port);
  void stop();

  bool running() const { return running_; }
  int port() const { return port_; }

 private:
  void run(int port);
  void handleClient(int clientFd);
  void processLine(const std::string& line);
  void sendMessage(const projection::core::RendererMessage& message);
  void closeClientSocket();

  RendererCommandHandler& handler_;
  std::atomic<bool> running_{false};
  int serverFd_{-1};
  int clientFd_{-1};
  int port_{0};
  std::thread serverThread_{};
  std::mutex socketMutex_{};
};

}  // namespace projection::renderer

