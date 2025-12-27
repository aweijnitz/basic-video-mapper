#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <netinet/in.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

#include <nlohmann/json.hpp>

#include "net/RendererClient.h"

using projection::core::RendererMessage;
using projection::core::RendererMessageType;

namespace {
class RecordingHandler : public projection::renderer::RendererCommandHandler {
 public:
  void handle(const RendererMessage& message) override {
    std::lock_guard<std::mutex> lock(mutex_);
    lastMessage_ = message;
    cv_.notify_all();
  }

  bool waitForMessage(std::chrono::milliseconds timeout = std::chrono::milliseconds(1000)) {
    std::unique_lock<std::mutex> lock(mutex_);
    return cv_.wait_for(lock, timeout, [this] { return lastMessage_.has_value(); });
  }

  std::optional<RendererMessage> lastMessage() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastMessage_;
  }

 private:
  mutable std::mutex mutex_;
  std::condition_variable cv_;
  std::optional<RendererMessage> lastMessage_;
};

class TestServer {
 public:
  TestServer() {
    listener_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listener_ < 0) {
      bindError_ = errno;
      return;
    }

    sockaddr_in addr{};
#ifdef __APPLE__
    addr.sin_len = sizeof(addr);
#endif
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(0);

    if (::bind(listener_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
      bindError_ = errno;
      ::close(listener_);
      listener_ = -1;
      return;
    }

    socklen_t len = sizeof(addr);
    if (::getsockname(listener_, reinterpret_cast<sockaddr*>(&addr), &len) != 0) {
      bindError_ = errno;
      ::close(listener_);
      listener_ = -1;
      return;
    }
    port_ = ntohs(addr.sin_port);

    if (::listen(listener_, 1) != 0) {
      bindError_ = errno;
      ::close(listener_);
      listener_ = -1;
      return;
    }

    serverThread_ = std::thread([this] { run(); });
    ready_ = true;
  }

  ~TestServer() {
    if (client_ >= 0) {
      ::close(client_);
    }
    if (listener_ >= 0) {
      ::shutdown(listener_, SHUT_RDWR);
      ::close(listener_);
    }
    if (serverThread_.joinable()) {
      serverThread_.join();
    }
  }

  int port() const { return port_; }
  bool isReady() const { return ready_; }
  int bindError() const { return bindError_; }

  RendererMessage helloMessage() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return helloMessage_;
  }

  RendererMessage ackMessage() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return ackMessage_;
  }

 private:
  void run() {
    sockaddr_in clientAddr{};
    socklen_t clientLen = sizeof(clientAddr);
    client_ = ::accept(listener_, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
    if (client_ < 0) {
      return;
    }

    std::string buffer;
    char chunk[256];
    while (true) {
      ssize_t received = ::recv(client_, chunk, sizeof(chunk), 0);
      if (received <= 0) {
        return;
      }
      buffer.append(chunk, static_cast<size_t>(received));
      auto newlinePos = buffer.find('\n');
      if (newlinePos == std::string::npos) {
        continue;
      }

      std::string line = buffer.substr(0, newlinePos);
      buffer.erase(0, newlinePos + 1);

      auto message = nlohmann::json::parse(line).get<RendererMessage>();
      {
        std::lock_guard<std::mutex> lock(mutex_);
        helloMessage_ = message;
      }

      RendererMessage ack{};
      ack.type = RendererMessageType::Ack;
      ack.commandId = message.commandId;
      ack.ack = projection::core::AckMessage{message.commandId};
      std::string response = nlohmann::json(ack).dump() + "\n";
      ::send(client_, response.c_str(), response.size(), 0);
      break;
    }

    RendererMessage loadScene{};
    loadScene.type = RendererMessageType::LoadScene;
    loadScene.commandId = "cmd-load";
    loadScene.loadScene = projection::core::LoadSceneMessage{projection::core::SceneId{"scene-1"}};
    std::string outbound = nlohmann::json(loadScene).dump() + "\n";
    ::send(client_, outbound.c_str(), outbound.size(), 0);

    std::string responseBuffer;
    while (true) {
      ssize_t received = ::recv(client_, chunk, sizeof(chunk), 0);
      if (received <= 0) {
        return;
      }
      responseBuffer.append(chunk, static_cast<size_t>(received));
      auto newlinePos = responseBuffer.find('\n');
      if (newlinePos == std::string::npos) {
        continue;
      }

      std::string line = responseBuffer.substr(0, newlinePos);
      auto response = nlohmann::json::parse(line).get<RendererMessage>();
      {
        std::lock_guard<std::mutex> lock(mutex_);
        ackMessage_ = response;
      }
      return;
    }
  }

  int listener_{-1};
  int client_{-1};
  int port_{0};
  std::thread serverThread_;
  mutable std::mutex mutex_;
  RendererMessage helloMessage_{};
  RendererMessage ackMessage_{};
  bool ready_{false};
  int bindError_{0};
};
}  // namespace

TEST_CASE("RendererClient connects, sends hello, and acknowledges commands", "[renderer][client]") {
  RecordingHandler handler;
  TestServer server;
  if (!server.isReady()) {
    std::cerr << "Skipping socket test: bind failed ("
              << std::strerror(server.bindError()) << ")" << std::endl;
    return;
  }
  projection::renderer::RendererClient client(handler, "127.0.0.1", server.port(), "studio-a", true);

  client.start();
  REQUIRE(handler.waitForMessage());

  auto hello = server.helloMessage();
  REQUIRE(hello.type == RendererMessageType::Hello);
  REQUIRE(hello.hello.has_value());
  REQUIRE(hello.hello->name == "studio-a");

  auto received = handler.lastMessage();
  REQUIRE(received.has_value());
  REQUIRE(received->type == RendererMessageType::LoadScene);
  REQUIRE(received->loadScene.has_value());
  REQUIRE(received->loadScene->sceneId.value == "scene-1");

  auto ack = server.ackMessage();
  REQUIRE(ack.type == RendererMessageType::Ack);
  REQUIRE(ack.commandId == "cmd-load");

  client.stop();
}
