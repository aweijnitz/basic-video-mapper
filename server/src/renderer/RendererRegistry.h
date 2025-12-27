#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "projection/core/RendererProtocol.h"

namespace projection::server::renderer {

class RendererSession;

class RendererRegistry {
public:
    explicit RendererRegistry(bool verbose = false);
    ~RendererRegistry();

    RendererRegistry(const RendererRegistry&) = delete;
    RendererRegistry& operator=(const RendererRegistry&) = delete;

    void start(int port);
    void stop();

    bool running() const { return running_; }
    int port() const { return port_; }
    std::vector<std::string> rendererNames() const;
    size_t rendererCount() const;

    size_t broadcastMessage(const projection::core::RendererMessage& message);

private:
    void run(int port);
    void handleClient(int clientFd);
    bool sendMessage(int clientFd, const projection::core::RendererMessage& message);

    std::atomic<bool> running_{false};
    bool verbose_{false};
    int serverFd_{-1};
    int port_{0};
    std::thread serverThread_{};
    mutable std::mutex sessionsMutex_{};
    std::unordered_map<std::string, std::shared_ptr<RendererSession>> sessions_{};
};

}  // namespace projection::server::renderer
