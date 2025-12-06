#pragma once

#include <string>
#include <vector>

#include "projection/core/RendererProtocol.h"

namespace projection::server::renderer {

class RendererClient {
public:
    RendererClient(std::string host, int port);
    ~RendererClient();

    void connect();
    void disconnect();

    void sendMessage(const projection::core::RendererMessage& msg);
    void sendLoadSceneDefinition(const projection::core::Scene& scene, const std::vector<projection::core::Feed>& feeds);
    projection::core::RendererMessage receiveMessage();

private:
    std::string host_;
    int port_;
    int socketFd_;

    void ensureConnected() const;
    std::string generateCommandId() const;
};

}  // namespace projection::server::renderer

