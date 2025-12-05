#pragma once

#include <httplib.h>
#include <memory>

#include "repo/FeedRepository.h"
#include "repo/SceneRepository.h"

namespace projection::server::http {

class HttpServer {
public:
    HttpServer(repo::FeedRepository& feedRepository, repo::SceneRepository& sceneRepository);

    // Starts listening on the provided port. This call blocks until stop() is invoked
    // from another thread.
    void start(int port);

    // Signals the server to stop listening. Supported by cpp-httplib.
    void stop();

    bool isRunning() const;

private:
    void registerRoutes();
    void respondWithError(::httplib::Response& res, int status, const std::string& message);

    repo::FeedRepository& feedRepository_;
    repo::SceneRepository& sceneRepository_;
    std::unique_ptr<::httplib::Server> server_;
};

}  // namespace projection::server::http
