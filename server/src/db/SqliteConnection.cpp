#include "db/SqliteConnection.h"

#include <sqlite3.h>

#include <stdexcept>
#include <utility>

namespace projection::server::db {

SqliteConnection::SqliteConnection() : handle_(nullptr) {}

SqliteConnection::~SqliteConnection() {
    if (handle_ != nullptr) {
        sqlite3_close(handle_);
        handle_ = nullptr;
    }
}

void SqliteConnection::open(const std::string& path) {
    if (handle_ != nullptr) {
        sqlite3_close(handle_);
        handle_ = nullptr;
    }

    sqlite3* db_handle = nullptr;
    int result = sqlite3_open(path.c_str(), &db_handle);
    if (result != SQLITE_OK) {
        throw std::runtime_error("Failed to open SQLite database: " + std::string(sqlite3_errstr(result)));
    }

    handle_ = db_handle;
}

sqlite3* SqliteConnection::getHandle() const { return handle_; }

void SqliteConnection::execute(const std::string& sql) const {
    char* errorMessage = nullptr;
    int result = sqlite3_exec(handle_, sql.c_str(), nullptr, nullptr, &errorMessage);
    if (result != SQLITE_OK) {
        std::string error = errorMessage ? errorMessage : "Unknown error";
        sqlite3_free(errorMessage);
        throw std::runtime_error("SQLite execution failed: " + error);
    }
}

}  // namespace projection::server::db
