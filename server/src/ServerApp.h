#pragma once

#include <string>
#include <memory>

namespace projection::server::db {
class SqliteConnection;
}

namespace projection::server::repo {
class FeedRepository;
class SceneRepository;
}

namespace projection::server::http {
class HttpServer;
}

namespace projection::server {

struct ServerConfig {
    std::string databasePath;
    int httpPort;
};

class ServerApp {
public:
    explicit ServerApp(ServerConfig config);
    ~ServerApp();

    int run();

    void stop();

private:
    ServerConfig config_;
    std::unique_ptr<db::SqliteConnection> connection_;
    std::unique_ptr<repo::FeedRepository> feedRepository_;
    std::unique_ptr<repo::SceneRepository> sceneRepository_;
    std::unique_ptr<http::HttpServer> httpServer_;
};

}  // namespace projection::server
