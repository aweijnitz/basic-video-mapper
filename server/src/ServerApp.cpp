#include "ServerApp.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>

#include "db/SchemaMigrations.h"
#include "db/SqliteConnection.h"
#include "http/HttpServer.h"
#include "repo/FeedRepository.h"
#include "repo/ProjectRepository.h"
#include "repo/SceneRepository.h"
#include "renderer/RendererRegistry.h"

namespace projection::server {

ServerApp::ServerApp(ServerConfig config) : config_(std::move(config)) {}

ServerApp::~ServerApp() = default;

int ServerApp::run() {
    try {
        auto log = [&](const std::string& msg) {
            if (config_.verbose) {
                std::cout << "[server] " << msg << std::endl;
            }
        };

        std::filesystem::path dbPath(config_.databasePath);
        if (!dbPath.parent_path().empty()) {
            std::filesystem::create_directories(dbPath.parent_path());
        }
        log("Opening database at " + dbPath.string());

        connection_ = std::make_unique<db::SqliteConnection>();
        connection_->open(dbPath.string());
        log("Applying schema migrations");
        db::SchemaMigrations::applyMigrations(*connection_);

        feedRepository_ = std::make_unique<repo::FeedRepository>(*connection_);
        sceneRepository_ = std::make_unique<repo::SceneRepository>(*connection_);
        cueRepository_ = std::make_unique<repo::CueRepository>(*connection_);
        projectRepository_ = std::make_unique<repo::ProjectRepository>(*connection_);

        rendererRegistry_ = std::make_shared<renderer::RendererRegistry>(config_.verbose);
        log("Listening for renderers on port " + std::to_string(config_.rendererPort));
        rendererRegistry_->start(config_.rendererPort);

        httpServer_ = std::make_unique<http::HttpServer>(*feedRepository_, *sceneRepository_, *cueRepository_,
                                                         *projectRepository_, rendererRegistry_, config_.verbose);
        std::cout << "Database initialized at '" << dbPath.string() << "'" << std::endl;
        std::cout << "HTTP server listening on port " << config_.httpPort << std::endl;
        httpServer_->start(config_.httpPort);
    } catch (const std::exception& ex) {
        std::cerr << "Failed to start server: " << ex.what() << std::endl;
        return 1;
    }

    std::cout << "Server initialization complete" << std::endl;
    return 0;
}

void ServerApp::stop() {
    if (rendererRegistry_) {
        rendererRegistry_->stop();
    }
    if (httpServer_) {
        httpServer_->stop();
    }
}

}  // namespace projection::server
