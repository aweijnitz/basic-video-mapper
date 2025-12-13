#include "http/HttpServer.h"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <stdexcept>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

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

    auto handleRendererPing = [this](const ::httplib::Request&, ::httplib::Response& res) {
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
    };

    server_->Post("/renderer/ping", handleRendererPing);
    server_->Get("/renderer/ping", handleRendererPing);

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
            auto scene = sceneRepository_.findSceneById(sceneId);
            if (!scene.has_value()) {
                respondWithError(res, 400, "Scene does not exist");
                return;
            }

            std::vector<core::Feed> feeds;
            std::string error;
            if (!collectFeedsForScene(*scene, feeds, error)) {
                respondWithError(res, 400, error);
                return;
            }

            rendererClient_->sendLoadSceneDefinition(*scene, feeds);
            res.status = 200;
            res.set_content(json({{"status", "sent"}}).dump(), "application/json");
        } catch (const json::exception& ex) {
            respondWithError(res, 400, ex.what());
        } catch (const std::exception& ex) {
            respondWithError(res, 500, ex.what());
        }
    });

    server_->Post("/demo/two-video-test", [this](const ::httplib::Request&, ::httplib::Response& res) {
        if (!rendererClient_) {
            respondWithError(res, 500, "Renderer client not configured");
            return;
        }

        try {
            const auto suffix = generateCommandId();
            core::Feed feedA(core::FeedId{}, "Demo Clip A", core::FeedType::VideoFile,
                             R"({"filePath":"data/assets/clipA.mp4"})");
            core::Feed feedB(core::FeedId{}, "Demo Clip B", core::FeedType::VideoFile,
                             R"({"filePath":"data/assets/clipB.mp4"})");

            feedA = feedRepository_.createFeed(feedA);
            feedB = feedRepository_.createFeed(feedB);

            std::vector<core::Vec2> quadA{{-0.8f, -0.6f}, {-0.1f, -0.5f}, {-0.1f, 0.2f}, {-0.8f, 0.1f}};
            std::vector<core::Vec2> quadB{{0.1f, -0.3f}, {0.8f, -0.2f}, {0.7f, 0.5f}, {0.0f, 0.4f}};

            core::Surface surfaceA(core::SurfaceId{"demo-surface-a-" + suffix}, "Demo Surface A", quadA, feedA.getId());
            core::Surface surfaceB(core::SurfaceId{"demo-surface-b-" + suffix}, "Demo Surface B", quadB, feedB.getId());

            core::Scene scene(core::SceneId{}, "Two Video Demo Scene", "Auto-generated demo scene",
                              std::vector<core::Surface>{surfaceA, surfaceB});

            auto feeds = feedRepository_.listFeeds();
            std::string validationError;
            if (!core::validateSceneFeeds(scene, feeds, validationError)) {
                respondWithError(res, 400, validationError);
                return;
            }

            auto createdScene = sceneRepository_.createScene(scene);

            std::vector<core::Feed> rendererFeeds;
            std::string error;
            if (!collectFeedsForScene(createdScene, rendererFeeds, error)) {
                respondWithError(res, 400, error);
                return;
            }

            rendererClient_->sendLoadSceneDefinition(createdScene, rendererFeeds);

            json payload{{"sceneId", createdScene.getId().value},
                         {"feedIds", json::array({feedA.getId().value, feedB.getId().value})},
                         {"surfaceIds", json::array({surfaceA.getId().value, surfaceB.getId().value})}};
            res.status = 200;
            res.set_content(payload.dump(), "application/json");
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

bool HttpServer::collectFeedsForScene(const core::Scene& scene, std::vector<core::Feed>& feeds, std::string& error) {
    std::vector<std::string> feedOrder;
    std::unordered_set<std::string> seenFeedIds;
    for (const auto& surface : scene.getSurfaces()) {
        const auto& feedIdValue = surface.getFeedId().value;
        if (!feedIdValue.empty() && seenFeedIds.insert(feedIdValue).second) {
            feedOrder.push_back(feedIdValue);
        }
    }

    std::unordered_map<std::string, core::Feed> feedsById;
    for (const auto& feed : feedRepository_.listFeeds()) {
        feedsById.emplace(feed.getId().value, feed);
    }

    feeds.clear();
    feeds.reserve(feedOrder.size());
    for (const auto& feedId : feedOrder) {
        auto it = feedsById.find(feedId);
        if (it == feedsById.end()) {
            error = "Feed not found: " + feedId;
            return false;
        }
        feeds.push_back(it->second);
    }

    return true;
}

}  // namespace projection::server::http
