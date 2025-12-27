#include "net/RendererClient.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace projection::renderer {
namespace {
constexpr int kInvalidSocket = -1;
}  // namespace

RendererClient::RendererClient(RendererCommandHandler& handler,
                               std::string host,
                               int port,
                               std::string name,
                               bool verbose)
    : handler_(handler),
      host_(std::move(host)),
      port_(port),
      name_(std::move(name)),
      verbose_(verbose) {}

RendererClient::~RendererClient() { stop(); }

void RendererClient::start() {
  if (running_) {
    return;
  }
  running_ = true;
  clientThread_ = std::thread(&RendererClient::run, this);
}

void RendererClient::stop() {
  running_ = false;
  {
    std::lock_guard<std::mutex> lock(socketMutex_);
    if (socketFd_ != kInvalidSocket) {
      ::shutdown(socketFd_, SHUT_RDWR);
      ::close(socketFd_);
      socketFd_ = kInvalidSocket;
    }
  }
  if (clientThread_.joinable()) {
    clientThread_.join();
  }
}

std::string RendererClient::lastError() const {
  std::lock_guard<std::mutex> lock(errorMutex_);
  return lastError_;
}

void RendererClient::run() {
  try {
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  addrinfo* result = nullptr;
  int rc = ::getaddrinfo(host_.c_str(), std::to_string(port_).c_str(), &hints, &result);
  if (rc != 0 || result == nullptr) {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_ = "Failed to resolve server host: " + std::string(gai_strerror(rc));
    running_ = false;
    return;
  }

  int sock = kInvalidSocket;
  for (addrinfo* rp = result; rp != nullptr; rp = rp->ai_next) {
    sock = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (sock == kInvalidSocket) {
      continue;
    }
    if (::connect(sock, rp->ai_addr, rp->ai_addrlen) == 0) {
      break;
    }
    ::close(sock);
    sock = kInvalidSocket;
  }
  ::freeaddrinfo(result);

  if (sock == kInvalidSocket) {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_ = "Failed to connect to server at " + host_ + ":" + std::to_string(port_);
    running_ = false;
    return;
  }

  {
    std::lock_guard<std::mutex> lock(socketMutex_);
    socketFd_ = sock;
  }

  projection::core::RendererMessage hello{};
  hello.type = projection::core::RendererMessageType::Hello;
  hello.commandId = generateCommandId();
  hello.hello = projection::core::HelloMessage{"0.1", "renderer", name_};

  sendMessage(hello);

  std::string buffer;
  char chunk[256];
  while (running_) {
    ssize_t received = ::recv(sock, chunk, sizeof(chunk), 0);
    if (received <= 0) {
      std::lock_guard<std::mutex> lock(errorMutex_);
      lastError_ = "Renderer connection closed during handshake";
      ::close(sock);
      {
        std::lock_guard<std::mutex> socketLock(socketMutex_);
        socketFd_ = kInvalidSocket;
      }
      running_ = false;
      return;
    }
    buffer.append(chunk, static_cast<size_t>(received));
    auto newlinePos = buffer.find('\n');
    if (newlinePos == std::string::npos) {
      continue;
    }
    std::string line = buffer.substr(0, newlinePos);
    buffer.erase(0, newlinePos + 1);

    auto response = parseRendererMessageLine(line);
    if (response.type == projection::core::RendererMessageType::Ack) {
      if (verbose_) {
        std::cerr << "[renderer] connected to server " << host_ << ":" << port_ << " as " << name_ << std::endl;
      }
      break;
    }
    if (response.type == projection::core::RendererMessageType::Error && response.error) {
      std::lock_guard<std::mutex> lock(errorMutex_);
      lastError_ = response.error->message;
      ::close(sock);
      {
        std::lock_guard<std::mutex> socketLock(socketMutex_);
        socketFd_ = kInvalidSocket;
      }
      running_ = false;
      return;
    }

    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_ = "Unexpected handshake response from server";
    ::close(sock);
    {
      std::lock_guard<std::mutex> socketLock(socketMutex_);
      socketFd_ = kInvalidSocket;
    }
    running_ = false;
    return;
  }

  readLoop();
  } catch (const std::exception& ex) {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_ = ex.what();
    running_ = false;
  }
}

void RendererClient::readLoop() {
  std::string buffer;
  char chunk[256];

  while (running_) {
    int socketFd = kInvalidSocket;
    {
      std::lock_guard<std::mutex> lock(socketMutex_);
      socketFd = socketFd_;
    }
    if (socketFd == kInvalidSocket) {
      break;
    }
    ssize_t received = ::recv(socketFd, chunk, sizeof(chunk), 0);
    if (received <= 0) {
      std::lock_guard<std::mutex> lock(errorMutex_);
      lastError_ = "Renderer connection closed";
      running_ = false;
      break;
    }
    buffer.append(chunk, static_cast<size_t>(received));
    auto newlinePos = buffer.find('\n');
    while (newlinePos != std::string::npos) {
      std::string line = buffer.substr(0, newlinePos);
      buffer.erase(0, newlinePos + 1);
      processLine(line);
      newlinePos = buffer.find('\n');
    }
  }
}

void RendererClient::processLine(const std::string& line) {
  try {
    if (verbose_) {
      std::cerr << "[renderer] received: " << line << std::endl;
    }
    auto message = parseRendererMessageLine(line);
    if (message.type == projection::core::RendererMessageType::Ack ||
        message.type == projection::core::RendererMessageType::Error) {
      return;
    }

    try {
      handler_.handle(message);
      sendAck(message.commandId);
    } catch (const std::exception& ex) {
      sendError(message.commandId, ex.what());
    }
  } catch (const std::exception& ex) {
    sendError("unknown", ex.what());
  }
}

void RendererClient::sendMessage(const projection::core::RendererMessage& message) {
  std::string payload = renderRendererMessageLine(message);
  payload.push_back('\n');

  std::lock_guard<std::mutex> lock(socketMutex_);
  if (socketFd_ == kInvalidSocket) {
    throw std::runtime_error("RendererClient is not connected");
  }

  const char* data = payload.c_str();
  size_t totalSent = 0;
  while (totalSent < payload.size()) {
    ssize_t sent = ::send(socketFd_, data + totalSent, payload.size() - totalSent, 0);
    if (sent <= 0) {
      throw std::runtime_error("Failed to send renderer message");
    }
    totalSent += static_cast<size_t>(sent);
  }
}

void RendererClient::sendAck(const std::string& commandId) {
  projection::core::RendererMessage message{};
  message.type = projection::core::RendererMessageType::Ack;
  message.commandId = commandId;
  message.ack = projection::core::AckMessage{commandId};
  sendMessage(message);
}

void RendererClient::sendError(const std::string& commandId, const std::string& errorText) {
  projection::core::RendererMessage message{};
  message.type = projection::core::RendererMessageType::Error;
  message.commandId = commandId;
  message.error = projection::core::ErrorMessage{commandId, errorText};
  sendMessage(message);
}

std::string RendererClient::generateCommandId() const {
  auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  std::ostringstream oss;
  oss << "cmd-" << now;
  return oss.str();
}

}  // namespace projection::renderer
