#pragma once

#include <map>
#include <string>
#include <vector>

#include <sqlite3.h>

namespace projection::server::db {

inline const char kDefaultDbPath[] = "./data/db/projection.db";

class SqliteConnection {
public:
    explicit SqliteConnection(const std::string& dbPath = kDefaultDbPath);
    ~SqliteConnection();

    SqliteConnection(const SqliteConnection&) = delete;
    SqliteConnection& operator=(const SqliteConnection&) = delete;

    void open();
    void close();

    void execute(const std::string& sql);
    void execute(const std::string& sql, const std::vector<std::string>& parameters);

    std::vector<std::map<std::string, std::string>>
    query(const std::string& sql, const std::vector<std::string>& parameters = {});

    sqlite3* handle();

private:
    void throwOnError(int resultCode, const std::string& context);
    void bindParameters(sqlite3_stmt* statement, const std::vector<std::string>& parameters);

    std::string dbPath;
    sqlite3* connection;
};

}  // namespace projection::server::db
