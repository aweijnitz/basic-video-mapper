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
//   --renderer-connect-retries <n> : Attempts to connect to renderer (default 30, 2s apart).
//   --verbose             : Enable verbose logging to stdout/stderr.
//
// Defaults:
//   databasePath = "./data/db/projection.db"
//   httpPort = 8080
//   rendererHost = "127.0.0.1"
//   rendererPort = 5050
//   rendererConnectRetries = 30
//   verbose = false
ServerConfig parseServerConfig(int argc, char* argv[]);

}  // namespace projection::server
