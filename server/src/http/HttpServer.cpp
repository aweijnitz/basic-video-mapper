#include "http/HttpServer.h"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <stdexcept>
#include <sstream>

#include "projection/core/Serialization.h"
#include "projection/core/RendererProtocol.h"
#include "projection/core/Validation.h"

namespace projection::server::http {

using nlohmann::json;

HttpServer::HttpServer(repo::FeedRepository& feedRepository, repo::SceneRepository& sceneRepository,
                       std::shared_ptr<renderer::RendererClient> rendererClient)
    : feedRepository_(feedRepository),
      sceneRepository_(sceneRepository),
      rendererClient_(std::move(rendererClient)),
      server_(std::make_unique<::httplib::Server>()) {
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

            auto feeds = feedRepository_.listFeeds();
            std::string error;
            if (!core::validateSceneFeeds(scene, feeds, error)) {
                respondWithError(res, 400, error);
                return;
            }

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

    server_->Get(R"(/scenes/(.+))", [this](const ::httplib::Request& req, ::httplib::Response& res) {
        try {
            if (req.matches.size() < 2) {
                respondWithError(res, 400, "Missing scene id");
                return;
            }
            core::SceneId sceneId{req.matches[1]};
            auto scene = sceneRepository_.findSceneById(sceneId);
            if (!scene.has_value()) {
                respondWithError(res, 404, "Scene not found");
                return;
            }

            res.status = 200;
            res.set_content(json(*scene).dump(), "application/json");
        } catch (const std::exception& ex) {
            respondWithError(res, 500, ex.what());
        }
    });

    server_->Post("/renderer/ping", [this](const ::httplib::Request&, ::httplib::Response& res) {
        if (!rendererClient_) {
            respondWithError(res, 500, "Renderer client not configured");
            return;
        }

        try {
            core::RendererMessage message{};
            message.type = core::RendererMessageType::Hello;
            message.commandId = generateCommandId();
            message.hello = core::HelloMessage{"0.1", "server"};

            rendererClient_->sendMessage(message);
            const auto response = rendererClient_->receiveMessage();

            if (response.type == core::RendererMessageType::Ack) {
                res.status = 200;
                res.set_content(json({{"status", "ok"}}).dump(), "application/json");
            } else if (response.type == core::RendererMessageType::Error && response.error) {
                respondWithError(res, 500, response.error->message);
            } else {
                respondWithError(res, 500, "Unexpected response from renderer");
            }
        } catch (const std::exception& ex) {
            respondWithError(res, 500, ex.what());
        }
    });

    server_->Post("/renderer/loadScene", [this](const ::httplib::Request& req, ::httplib::Response& res) {
        if (!rendererClient_) {
            respondWithError(res, 500, "Renderer client not configured");
            return;
        }

        try {
            auto body = json::parse(req.body);
            if (!body.contains("sceneId") || !body["sceneId"].is_string()) {
                respondWithError(res, 400, "Missing or invalid sceneId");
                return;
            }

            core::SceneId sceneId{body["sceneId"].get<std::string>()};
            if (!sceneRepository_.sceneExists(sceneId)) {
                respondWithError(res, 400, "Scene does not exist");
                return;
            }

            core::RendererMessage message{};
            message.type = core::RendererMessageType::LoadScene;
            message.commandId = generateCommandId();
            message.loadScene = core::LoadSceneMessage{sceneId};

            rendererClient_->sendMessage(message);
            res.status = 200;
            res.set_content(json({{"status", "sent"}}).dump(), "application/json");
        } catch (const json::exception& ex) {
            respondWithError(res, 400, ex.what());
        } catch (const std::exception& ex) {
            respondWithError(res, 500, ex.what());
        }
    });
}

void HttpServer::respondWithError(::httplib::Response& res, int status, const std::string& message) {
    res.status = status;
    res.set_content(json({{"error", message}}).dump(), "application/json");
}

std::string HttpServer::generateCommandId() const {
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    std::ostringstream oss;
    oss << "cmd-" << now;
    return oss.str();
}

}  // namespace projection::server::http
