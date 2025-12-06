#include "renderer/RendererClient.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <chrono>
#include <stdexcept>
#include <string>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>

#include <nlohmann/json.hpp>

namespace projection::server::renderer {
namespace {
constexpr int kInvalidSocket = -1;
}

RendererClient::RendererClient(std::string host, int port)
    : host_(std::move(host)), port_(port), socketFd_(kInvalidSocket) {}

RendererClient::~RendererClient() { disconnect(); }

void RendererClient::ensureConnected() const {
    if (socketFd_ == kInvalidSocket) {
        throw std::runtime_error("RendererClient is not connected");
    }
}

void RendererClient::connect() {
    if (socketFd_ != kInvalidSocket) {
        return;
    }

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* result = nullptr;
    int rc = ::getaddrinfo(host_.c_str(), std::to_string(port_).c_str(), &hints, &result);
    if (rc != 0 || result == nullptr) {
        throw std::runtime_error("Failed to resolve renderer host: " + std::string(gai_strerror(rc)));
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
        throw std::runtime_error("Failed to connect to renderer at " + host_ + ":" + std::to_string(port_));
    }

    socketFd_ = sock;
}

void RendererClient::disconnect() {
    if (socketFd_ != kInvalidSocket) {
        ::close(socketFd_);
        socketFd_ = kInvalidSocket;
    }
}

void RendererClient::sendMessage(const projection::core::RendererMessage& msg) {
    ensureConnected();
    nlohmann::json j = msg;
    std::string payload = j.dump();
    payload.push_back('\n');

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

void RendererClient::sendLoadSceneDefinition(const projection::core::Scene& scene,
                                             const std::vector<projection::core::Feed>& feeds) {
    projection::core::RendererMessage message{};
    message.type = projection::core::RendererMessageType::LoadSceneDefinition;
    message.commandId = generateCommandId();
    message.loadSceneDefinition = projection::core::LoadSceneDefinitionMessage{scene, feeds};

    sendMessage(message);
}

projection::core::RendererMessage RendererClient::receiveMessage() {
    ensureConnected();
    std::string buffer;
    char chunk[256];

    while (true) {
        ssize_t received = ::recv(socketFd_, chunk, sizeof(chunk), 0);
        if (received < 0) {
            throw std::runtime_error("Failed to receive renderer message");
        }
        if (received == 0) {
            throw std::runtime_error("Renderer connection closed");
        }

        buffer.append(chunk, static_cast<size_t>(received));
        auto newlinePos = buffer.find('\n');
        if (newlinePos != std::string::npos) {
            std::string line = buffer.substr(0, newlinePos);
            try {
                auto json = nlohmann::json::parse(line);
                return json.get<projection::core::RendererMessage>();
            } catch (const nlohmann::json::exception& e) {
                throw std::runtime_error(std::string("Failed to parse renderer message: ") + e.what());
            }
        }
    }
}

std::string RendererClient::generateCommandId() const {
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    std::ostringstream oss;
    oss << "cmd-" << now;
    return oss.str();
}

}  // namespace projection::server::renderer

