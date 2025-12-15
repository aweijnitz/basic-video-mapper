#pragma once

#include <httplib.h>
#include <memory>

#include "repo/FeedRepository.h"
#include "repo/SceneRepository.h"
#include "repo/CueRepository.h"
#include "renderer/RendererClient.h"

namespace projection::server::http {

class HttpServer {
public:
    HttpServer(repo::FeedRepository& feedRepository, repo::SceneRepository& sceneRepository,
               repo::CueRepository& cueRepository, std::shared_ptr<renderer::RendererClient> rendererClient = nullptr,
               bool verbose = false);

    // Starts listening on the provided port. This call blocks until stop() is invoked
    // from another thread.
    void start(int port);

    // Signals the server to stop listening. Supported by cpp-httplib.
    void stop();

    bool isRunning() const;

private:
    void registerRoutes();
    void respondWithError(::httplib::Response& res, int status, const std::string& message);
    bool collectFeedsForScene(const core::Scene& scene, std::vector<core::Feed>& feeds, std::string& error);

    std::string generateCommandId() const;

    repo::FeedRepository& feedRepository_;
    repo::SceneRepository& sceneRepository_;
    repo::CueRepository& cueRepository_;
    std::shared_ptr<renderer::RendererClient> rendererClient_;
    std::unique_ptr<::httplib::Server> server_;
    bool verbose_{false};
};

}  // namespace projection::server::http
