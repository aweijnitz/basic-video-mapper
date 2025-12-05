#include "RendererServer.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <nlohmann/json.hpp>
#include <stdexcept>

using projection::core::RendererMessage;
using projection::core::RendererMessageType;

namespace projection::renderer {

RendererMessage parseRendererMessageLine(const std::string& line) {
  auto json = nlohmann::json::parse(line);
  return json.get<RendererMessage>();
}

std::string renderRendererMessageLine(const RendererMessage& message) {
  nlohmann::json json = message;
  return json.dump();
}

RendererMessage makeAckMessage(const std::string& commandId) {
  RendererMessage message{};
  message.type = RendererMessageType::Ack;
  message.commandId = commandId;
  projection::core::AckMessage ack{commandId};
  message.ack = ack;
  return message;
}

RendererMessage makeErrorMessage(const std::string& commandId, const std::string& errorText) {
  RendererMessage message{};
  message.type = RendererMessageType::Error;
  message.commandId = commandId;
  projection::core::ErrorMessage error{commandId, errorText};
  message.error = error;
  return message;
}

RendererServer::RendererServer(RendererCommandHandler& handler) : handler_(handler) {}

RendererServer::~RendererServer() { stop(); }

void RendererServer::start(int port) {
  if (running_) {
    return;
  }
  running_ = true;
  serverThread_ = std::thread(&RendererServer::run, this, port);
}

void RendererServer::stop() {
  running_ = false;
  closeClientSocket();
  if (serverFd_ >= 0) {
    shutdown(serverFd_, SHUT_RDWR);
    close(serverFd_);
    serverFd_ = -1;
  }
  if (serverThread_.joinable()) {
    serverThread_.join();
  }
}

void RendererServer::run(int port) {
  serverFd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (serverFd_ < 0) {
    running_ = false;
    throw std::runtime_error("Failed to create socket: " + std::string(strerror(errno)));
  }

  int opt = 1;
  setsockopt(serverFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons(port);

  if (bind(serverFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    running_ = false;
    throw std::runtime_error("Failed to bind socket: " + std::string(strerror(errno)));
  }

  socklen_t addrLen = sizeof(addr);
  if (getsockname(serverFd_, reinterpret_cast<sockaddr*>(&addr), &addrLen) == 0) {
    port_ = ntohs(addr.sin_port);
  }

  if (listen(serverFd_, 1) < 0) {
    running_ = false;
    throw std::runtime_error("Failed to listen on socket: " + std::string(strerror(errno)));
  }

  while (running_) {
    int client = accept(serverFd_, nullptr, nullptr);
    if (client < 0) {
      if (!running_) {
        break;
      }
      continue;
    }

    {
      std::lock_guard<std::mutex> lock(socketMutex_);
      clientFd_ = client;
    }

    handleClient(client);
    closeClientSocket();
  }
}

void RendererServer::handleClient(int clientFd) {
  std::string buffer;
  constexpr size_t kBufferSize = 1024;
  char data[kBufferSize];

  while (running_) {
    ssize_t received = recv(clientFd, data, kBufferSize, 0);
    if (received <= 0) {
      break;
    }
    buffer.append(data, static_cast<size_t>(received));

    size_t newlinePos;
    while ((newlinePos = buffer.find('\n')) != std::string::npos) {
      std::string line = buffer.substr(0, newlinePos);
      buffer.erase(0, newlinePos + 1);
      if (!line.empty() && line.back() == '\r') {
        line.pop_back();
      }
      if (!line.empty()) {
        processLine(line);
      }
    }
  }
}

void RendererServer::processLine(const std::string& line) {
  try {
    auto message = parseRendererMessageLine(line);
    handler_.handle(message);
    sendMessage(makeAckMessage(message.commandId));
  } catch (const std::exception& ex) {
    std::string commandId;
    try {
      auto json = nlohmann::json::parse(line);
      if (json.contains("commandId")) {
        commandId = json["commandId"].get<std::string>();
      }
    } catch (...) {
    }
    sendMessage(makeErrorMessage(commandId, ex.what()));
  }
}

void RendererServer::sendMessage(const RendererMessage& message) {
  std::string serialized = renderRendererMessageLine(message) + "\n";
  std::lock_guard<std::mutex> lock(socketMutex_);
  if (clientFd_ < 0) {
    return;
  }
  send(clientFd_, serialized.c_str(), serialized.size(), 0);
}

void RendererServer::closeClientSocket() {
  std::lock_guard<std::mutex> lock(socketMutex_);
  if (clientFd_ >= 0) {
    shutdown(clientFd_, SHUT_RDWR);
    close(clientFd_);
    clientFd_ = -1;
  }
}

}  // namespace projection::renderer

