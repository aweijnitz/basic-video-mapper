#include "renderer/RendererRegistry.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <functional>
#include <iostream>
#include <vector>
#include <nlohmann/json.hpp>

namespace projection::server::renderer {
namespace {
constexpr int kInvalidSocket = -1;

projection::core::RendererMessage parseRendererMessageLine(const std::string& line) {
    auto json = nlohmann::json::parse(line);
    return json.get<projection::core::RendererMessage>();
}

std::string renderRendererMessageLine(const projection::core::RendererMessage& message) {
    return nlohmann::json(message).dump();
}

projection::core::RendererMessage makeAckMessage(const std::string& commandId) {
    projection::core::RendererMessage message{};
    message.type = projection::core::RendererMessageType::Ack;
    message.commandId = commandId;
    message.ack = projection::core::AckMessage{commandId};
    return message;
}

projection::core::RendererMessage makeErrorMessage(const std::string& commandId, const std::string& errorText) {
    projection::core::RendererMessage message{};
    message.type = projection::core::RendererMessageType::Error;
    message.commandId = commandId;
    message.error = projection::core::ErrorMessage{commandId, errorText};
    return message;
}

}  // namespace

class RendererSession : public std::enable_shared_from_this<RendererSession> {
public:
    RendererSession(std::string name,
                    int socketFd,
                    bool verbose,
                    std::function<void(const std::string&)> onDisconnect)
        : name_(std::move(name)),
          socketFd_(socketFd),
          verbose_(verbose),
          onDisconnect_(std::move(onDisconnect)) {}

    ~RendererSession() { stop(); }

    RendererSession(const RendererSession&) = delete;
    RendererSession& operator=(const RendererSession&) = delete;

    const std::string& name() const { return name_; }

    void start() {
        if (running_) {
            return;
        }
        running_ = true;
        auto self = shared_from_this();
        readerThread_ = std::thread([self] { self->readLoop(); });
    }

    void stop() {
        running_ = false;
        if (socketFd_ != kInvalidSocket) {
            ::shutdown(socketFd_, SHUT_RDWR);
            ::close(socketFd_);
            socketFd_ = kInvalidSocket;
        }
        if (readerThread_.joinable()) {
            if (readerThread_.get_id() == std::this_thread::get_id()) {
                readerThread_.detach();
            } else {
                readerThread_.join();
            }
        }
    }

    bool sendMessage(const projection::core::RendererMessage& message) {
        std::lock_guard<std::mutex> lock(sendMutex_);
        if (socketFd_ == kInvalidSocket) {
            return false;
        }
        std::string payload = renderRendererMessageLine(message);
        payload.push_back('\n');

        const char* data = payload.c_str();
        size_t totalSent = 0;
        while (totalSent < payload.size()) {
            ssize_t sent = ::send(socketFd_, data + totalSent, payload.size() - totalSent, 0);
            if (sent <= 0) {
                return false;
            }
            totalSent += static_cast<size_t>(sent);
        }
        return true;
    }

private:
    void readLoop() {
        std::string buffer;
        char chunk[256];

        while (running_) {
            ssize_t received = ::recv(socketFd_, chunk, sizeof(chunk), 0);
            if (received <= 0) {
                break;
            }
            buffer.append(chunk, static_cast<size_t>(received));
            auto newlinePos = buffer.find('\n');
            while (newlinePos != std::string::npos) {
                std::string line = buffer.substr(0, newlinePos);
                buffer.erase(0, newlinePos + 1);
                if (verbose_) {
                    std::cerr << "[renderer-registry] received from " << name_ << ": " << line << std::endl;
                }
                newlinePos = buffer.find('\n');
            }
        }

        if (onDisconnect_) {
            onDisconnect_(name_);
        }
    }

    std::string name_;
    int socketFd_{kInvalidSocket};
    std::atomic<bool> running_{false};
    bool verbose_{false};
    std::function<void(const std::string&)> onDisconnect_{};
    std::thread readerThread_{};
    std::mutex sendMutex_{};
};

RendererRegistry::RendererRegistry(bool verbose) : verbose_(verbose) {}

RendererRegistry::~RendererRegistry() { stop(); }

void RendererRegistry::start(int port) {
    if (running_) {
        return;
    }
    running_ = true;
    serverThread_ = std::thread(&RendererRegistry::run, this, port);
}

void RendererRegistry::stop() {
    running_ = false;
    if (serverFd_ != kInvalidSocket) {
        ::shutdown(serverFd_, SHUT_RDWR);
        ::close(serverFd_);
        serverFd_ = kInvalidSocket;
    }

    std::vector<std::shared_ptr<RendererSession>> sessions;
    {
        std::lock_guard<std::mutex> lock(sessionsMutex_);
        for (auto& [_, session] : sessions_) {
            sessions.push_back(session);
        }
        sessions_.clear();
    }
    for (auto& session : sessions) {
        session->stop();
    }

    if (serverThread_.joinable()) {
        serverThread_.join();
    }
}

std::vector<std::string> RendererRegistry::rendererNames() const {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    std::vector<std::string> names;
    names.reserve(sessions_.size());
    for (const auto& [name, _] : sessions_) {
        names.push_back(name);
    }
    return names;
}

size_t RendererRegistry::rendererCount() const {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    return sessions_.size();
}

size_t RendererRegistry::broadcastMessage(const projection::core::RendererMessage& message) {
    std::vector<std::shared_ptr<RendererSession>> sessions;
    {
        std::lock_guard<std::mutex> lock(sessionsMutex_);
        for (const auto& [_, session] : sessions_) {
            sessions.push_back(session);
        }
    }

    size_t sentCount = 0;
    for (const auto& session : sessions) {
        if (session->sendMessage(message)) {
            ++sentCount;
        } else {
            std::lock_guard<std::mutex> lock(sessionsMutex_);
            sessions_.erase(session->name());
        }
    }
    return sentCount;
}

void RendererRegistry::run(int port) {
    serverFd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd_ < 0) {
        running_ = false;
        return;
    }

    int opt = 1;
    ::setsockopt(serverFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<uint16_t>(port));

    if (::bind(serverFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        ::close(serverFd_);
        serverFd_ = kInvalidSocket;
        running_ = false;
        return;
    }

    if (::listen(serverFd_, 8) != 0) {
        ::close(serverFd_);
        serverFd_ = kInvalidSocket;
        running_ = false;
        return;
    }

    socklen_t len = sizeof(addr);
    if (::getsockname(serverFd_, reinterpret_cast<sockaddr*>(&addr), &len) == 0) {
        port_ = ntohs(addr.sin_port);
    }

    if (verbose_) {
        std::cerr << "[renderer-registry] listening on 0.0.0.0:" << port_ << std::endl;
    }

    while (running_) {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        int clientFd = ::accept(serverFd_, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
        if (clientFd < 0) {
            if (running_) {
                continue;
            }
            break;
        }
        std::thread(&RendererRegistry::handleClient, this, clientFd).detach();
    }
}

void RendererRegistry::handleClient(int clientFd) {
    std::string buffer;
    char chunk[256];
    while (true) {
        ssize_t received = ::recv(clientFd, chunk, sizeof(chunk), 0);
        if (received <= 0) {
            ::close(clientFd);
            return;
        }
        buffer.append(chunk, static_cast<size_t>(received));
        auto newlinePos = buffer.find('\n');
        if (newlinePos == std::string::npos) {
            continue;
        }

        std::string line = buffer.substr(0, newlinePos);
        try {
            auto message = parseRendererMessageLine(line);
            if (message.type != projection::core::RendererMessageType::Hello || !message.hello) {
                auto error = makeErrorMessage(message.commandId, "Expected hello message");
                sendMessage(clientFd, error);
                ::close(clientFd);
                return;
            }

            const auto& hello = *message.hello;
            if (hello.name.empty()) {
                auto error = makeErrorMessage(message.commandId, "Renderer name must be provided");
                sendMessage(clientFd, error);
                ::close(clientFd);
                return;
            }

            auto session = std::make_shared<RendererSession>(
                hello.name, clientFd, verbose_, [this](const std::string& name) {
                    std::lock_guard<std::mutex> lock(sessionsMutex_);
                    sessions_.erase(name);
                });
            {
                std::lock_guard<std::mutex> lock(sessionsMutex_);
                if (sessions_.find(hello.name) != sessions_.end()) {
                    auto error = makeErrorMessage(message.commandId, "Renderer name already in use");
                    sendMessage(clientFd, error);
                    ::close(clientFd);
                    return;
                }
                sessions_.emplace(hello.name, session);
            }

            auto ack = makeAckMessage(message.commandId);
            if (!sendMessage(clientFd, ack)) {
                std::lock_guard<std::mutex> lock(sessionsMutex_);
                sessions_.erase(hello.name);
                ::close(clientFd);
                return;
            }

            if (verbose_) {
                std::cerr << "[renderer-registry] registered renderer '" << hello.name << "'" << std::endl;
            }

            session->start();
            return;
        } catch (const std::exception& ex) {
            auto error = makeErrorMessage("handshake", ex.what());
            sendMessage(clientFd, error);
            ::close(clientFd);
            return;
        }
    }
}

bool RendererRegistry::sendMessage(int clientFd, const projection::core::RendererMessage& message) {
    std::string payload = renderRendererMessageLine(message);
    payload.push_back('\n');
    const char* data = payload.c_str();
    size_t totalSent = 0;
    while (totalSent < payload.size()) {
        ssize_t sent = ::send(clientFd, data + totalSent, payload.size() - totalSent, 0);
        if (sent <= 0) {
            return false;
        }
        totalSent += static_cast<size_t>(sent);
    }
    return true;
}

}  // namespace projection::server::renderer
