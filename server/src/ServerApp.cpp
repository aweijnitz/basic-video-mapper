#include "ServerApp.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>

#include "db/SchemaMigrations.h"
#include "db/SqliteConnection.h"

namespace projection::server {

ServerApp::ServerApp(ServerConfig config) : config_(std::move(config)) {}

int ServerApp::run() {
    try {
        std::filesystem::path dbPath(config_.databasePath);
        if (!dbPath.parent_path().empty()) {
            std::filesystem::create_directories(dbPath.parent_path());
        }

        db::SqliteConnection connection;
        connection.open(dbPath.string());
        db::SchemaMigrations::applyMigrations(connection);

        std::cout << "Database initialized at '" << dbPath.string() << "'" << std::endl;
    } catch (const std::exception& ex) {
        std::cerr << "Failed to start server: " << ex.what() << std::endl;
        return 1;
    }

    std::cout << "Server initialization complete" << std::endl;
    return 0;
}

}  // namespace projection::server
