#include "ServerApp.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>

#include "db/SchemaMigrations.h"
#include "db/SqliteConnection.h"
#include "http/HttpServer.h"
#include "repo/FeedRepository.h"
#include "repo/SceneRepository.h"
#include "renderer/RendererClient.h"

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

        rendererClient_ = std::make_shared<renderer::RendererClient>(config_.rendererHost, config_.rendererPort);
        log("Connecting to renderer at " + config_.rendererHost + ":" + std::to_string(config_.rendererPort));
        bool connected = false;
        for (int attempt = 1; attempt <= config_.rendererConnectRetries; ++attempt) {
            try {
                rendererClient_->connect();
                connected = true;
                break;
            } catch (const std::exception& ex) {
                std::cerr << "Renderer connect attempt " << attempt << "/" << config_.rendererConnectRetries
                          << " failed: " << ex.what() << std::endl;
                if (attempt == config_.rendererConnectRetries) {
                    throw;
                }
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
        }
        if (connected) {
            std::cout << "Connected to renderer at " << config_.rendererHost << ":" << config_.rendererPort
                      << std::endl;
        }

        httpServer_ = std::make_unique<http::HttpServer>(*feedRepository_, *sceneRepository_, *cueRepository_,
                                                         rendererClient_, config_.verbose);
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
    if (httpServer_) {
        httpServer_->stop();
    }
}

}  // namespace projection::server
