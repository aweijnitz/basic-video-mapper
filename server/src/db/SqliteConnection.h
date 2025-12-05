#pragma once

#include <string>

struct sqlite3;

namespace projection::server::db {

class SqliteConnection {
public:
    SqliteConnection();
    ~SqliteConnection();

    void open(const std::string& path);

    sqlite3* getHandle() const;

    void execute(const std::string& sql) const;

private:
    sqlite3* handle_;
};

}  // namespace projection::server::db
