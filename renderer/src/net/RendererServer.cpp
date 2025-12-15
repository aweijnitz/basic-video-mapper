#include "RendererServer.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <iostream>
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

RendererServer::RendererServer(RendererCommandHandler& handler, bool verbose) : handler_(handler), verbose_(verbose) {}

RendererServer::~RendererServer() { stop(); }

void RendererServer::start(int port) {
  if (running_) {
    return;
  }
  {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_.clear();
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

std::string RendererServer::lastError() const {
  std::lock_guard<std::mutex> lock(errorMutex_);
  return lastError_;
}

void RendererServer::run(int port) {
  try {
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

    if (verbose_) {
      std::cerr << "RendererServer listening on 127.0.0.1:" << port_ << std::endl;
    }

    while (running_) {
      int client = accept(serverFd_, nullptr, nullptr);
      if (client < 0) {
        if (!running_) {
          break;
        }
        continue;
      }

      if (verbose_) {
        std::cerr << "RendererServer accepted client" << std::endl;
      }
      {
        std::lock_guard<std::mutex> lock(socketMutex_);
        clientFd_ = client;
      }

      handleClient(client);
      closeClientSocket();
      if (verbose_) {
        std::cerr << "RendererServer closed client" << std::endl;
      }
    }
  } catch (const std::exception& ex) {
    running_ = false;
    {
      std::lock_guard<std::mutex> lock(errorMutex_);
      lastError_ = ex.what();
    }
    if (verbose_) {
      std::cerr << "RendererServer failed: " << ex.what() << std::endl;
    }
  } catch (...) {
    running_ = false;
    {
      std::lock_guard<std::mutex> lock(errorMutex_);
      lastError_ = "Unknown error";
    }
    if (verbose_) {
      std::cerr << "RendererServer failed with unknown error" << std::endl;
    }
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
    if (verbose_) {
      std::cerr << "RendererServer read " << received << " bytes" << std::endl;
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
    if (verbose_) {
      std::cerr << "RendererServer received: " << line << std::endl;
    }
    auto message = parseRendererMessageLine(line);
    handler_.handle(message);
    sendMessage(makeAckMessage(message.commandId));
    if (verbose_) {
      std::cerr << "RendererServer sent Ack for " << message.commandId << std::endl;
    }
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
    if (verbose_) {
      std::cerr << "RendererServer sent Error for " << commandId << ": " << ex.what() << std::endl;
    }
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
