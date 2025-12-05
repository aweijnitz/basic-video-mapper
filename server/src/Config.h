#pragma once

#include "ServerApp.h"

namespace projection::server {

// Parses command-line arguments for server configuration. Supported options:
//   --db <path>    : Path to the SQLite database file.
//   --db=<path>
//   --port <port>          : HTTP port to listen on.
//   --port=<port>
//   --renderer-host <host> : Renderer hostname or IP.
//   --renderer-host=<host>
//   --renderer-port <port> : Renderer TCP port.
//   --renderer-port=<port>
//
// Defaults:
//   databasePath = "./data/db/projection.db"
//   httpPort = 8080
//   rendererHost = "127.0.0.1"
//   rendererPort = 5050
ServerConfig parseServerConfig(int argc, char* argv[]);

}  // namespace projection::server
