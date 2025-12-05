#include "http/HttpServer.h"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <stdexcept>

#include "projection/core/Serialization.h"

namespace projection::server::http {

using nlohmann::json;

HttpServer::HttpServer(repo::FeedRepository& feedRepository, repo::SceneRepository& sceneRepository)
    : feedRepository_(feedRepository), sceneRepository_(sceneRepository), server_(std::make_unique<::httplib::Server>()) {
    registerRoutes();
}

void HttpServer::start(int port) {
    if (!server_->listen("0.0.0.0", port)) {
        throw std::runtime_error("Failed to start HTTP server on port " + std::to_string(port));
    }
}

void HttpServer::stop() { server_->stop(); }

bool HttpServer::isRunning() const { return server_ && server_->is_running(); }

void HttpServer::registerRoutes() {
    server_->Post("/feeds", [this](const ::httplib::Request& req, ::httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            auto feed = body.get<core::Feed>();
            auto created = feedRepository_.createFeed(feed);
            res.status = 201;
            res.set_content(json(created).dump(), "application/json");
        } catch (const std::exception& ex) {
            respondWithError(res, 400, ex.what());
        }
    });

    server_->Get("/feeds", [this](const ::httplib::Request&, ::httplib::Response& res) {
        try {
            const auto feeds = feedRepository_.listFeeds();
            res.status = 200;
            res.set_content(json(feeds).dump(), "application/json");
        } catch (const std::exception& ex) {
            respondWithError(res, 500, ex.what());
        }
    });

    server_->Post("/scenes", [this](const ::httplib::Request& req, ::httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            auto scene = body.get<core::Scene>();
            auto created = sceneRepository_.createScene(scene);
            res.status = 201;
            res.set_content(json(created).dump(), "application/json");
        } catch (const std::exception& ex) {
            respondWithError(res, 400, ex.what());
        }
    });

    server_->Get("/scenes", [this](const ::httplib::Request&, ::httplib::Response& res) {
        try {
            const auto scenes = sceneRepository_.listScenes();
            res.status = 200;
            res.set_content(json(scenes).dump(), "application/json");
        } catch (const std::exception& ex) {
            respondWithError(res, 500, ex.what());
        }
    });
}

void HttpServer::respondWithError(::httplib::Response& res, int status, const std::string& message) {
    res.status = status;
    res.set_content(json({{"error", message}}).dump(), "application/json");
}

}  // namespace projection::server::http
