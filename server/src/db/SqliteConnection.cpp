#include "db/SqliteConnection.h"

#include <filesystem>
#include <stdexcept>

namespace projection::server::db {

SqliteConnection::SqliteConnection(const std::string& dbPath)
    : dbPath(dbPath), connection(nullptr) {}

SqliteConnection::~SqliteConnection() { close(); }

void SqliteConnection::throwOnError(int resultCode, const std::string& context) {
    if (resultCode != SQLITE_OK && resultCode != SQLITE_DONE && resultCode != SQLITE_ROW) {
        const char* message = connection != nullptr ? sqlite3_errmsg(connection) : sqlite3_errstr(resultCode);
        throw std::runtime_error(context + ": " + message);
    }
}

void SqliteConnection::bindParameters(sqlite3_stmt* statement, const std::vector<std::string>& parameters) {
    for (int i = 0; i < static_cast<int>(parameters.size()); ++i) {
        int bindResult = sqlite3_bind_text(statement, i + 1, parameters[i].c_str(), -1, SQLITE_TRANSIENT);
        throwOnError(bindResult, "bind parameter " + std::to_string(i + 1));
    }
}

void SqliteConnection::open() {
    if (connection != nullptr) {
        return;
    }

    std::filesystem::path path(dbPath);
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }

    int result = sqlite3_open(dbPath.c_str(), &connection);
    throwOnError(result, "open database");
}

void SqliteConnection::close() {
    if (connection != nullptr) {
        sqlite3_close(connection);
        connection = nullptr;
    }
}

void SqliteConnection::execute(const std::string& sql) { execute(sql, {}); }

void SqliteConnection::execute(const std::string& sql, const std::vector<std::string>& parameters) {
    if (connection == nullptr) {
        throw std::runtime_error("database not opened");
    }

    sqlite3_stmt* statement = nullptr;
    int prepareResult = sqlite3_prepare_v2(connection, sql.c_str(), -1, &statement, nullptr);
    throwOnError(prepareResult, "prepare statement");

    bindParameters(statement, parameters);

    int stepResult = sqlite3_step(statement);
    throwOnError(stepResult, "execute statement");

    sqlite3_finalize(statement);
}

std::vector<std::map<std::string, std::string>>
SqliteConnection::query(const std::string& sql, const std::vector<std::string>& parameters) {
    if (connection == nullptr) {
        throw std::runtime_error("database not opened");
    }

    sqlite3_stmt* statement = nullptr;
    int prepareResult = sqlite3_prepare_v2(connection, sql.c_str(), -1, &statement, nullptr);
    throwOnError(prepareResult, "prepare statement");

    bindParameters(statement, parameters);

    std::vector<std::map<std::string, std::string>> rows;

    int columnCount = sqlite3_column_count(statement);
    int stepResult = sqlite3_step(statement);
    while (stepResult == SQLITE_ROW) {
        std::map<std::string, std::string> row;
        for (int columnIndex = 0; columnIndex < columnCount; ++columnIndex) {
            const char* columnName = sqlite3_column_name(statement, columnIndex);
            const unsigned char* text = sqlite3_column_text(statement, columnIndex);
            std::string value = text != nullptr ? reinterpret_cast<const char*>(text) : "";
            row[columnName] = value;
        }
        rows.push_back(row);
        stepResult = sqlite3_step(statement);
    }

    throwOnError(stepResult, "iterate query results");

    sqlite3_finalize(statement);

    return rows;
}

sqlite3* SqliteConnection::handle() { return connection; }

}  // namespace projection::server::db
