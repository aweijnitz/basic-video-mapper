#include "ServerApp.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>

#include "db/SchemaMigrations.h"
#include "db/SqliteConnection.h"
#include "http/HttpServer.h"
#include "repo/FeedRepository.h"
#include "repo/SceneRepository.h"

namespace projection::server {

ServerApp::ServerApp(ServerConfig config) : config_(std::move(config)) {}

ServerApp::~ServerApp() = default;

int ServerApp::run() {
    try {
        std::filesystem::path dbPath(config_.databasePath);
        if (!dbPath.parent_path().empty()) {
            std::filesystem::create_directories(dbPath.parent_path());
        }

        connection_ = std::make_unique<db::SqliteConnection>();
        connection_->open(dbPath.string());
        db::SchemaMigrations::applyMigrations(*connection_);

        feedRepository_ = std::make_unique<repo::FeedRepository>(*connection_);
        sceneRepository_ = std::make_unique<repo::SceneRepository>(*connection_);

        std::cout << "Database initialized at '" << dbPath.string() << "'" << std::endl;
        httpServer_ = std::make_unique<http::HttpServer>(*feedRepository_, *sceneRepository_);
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
