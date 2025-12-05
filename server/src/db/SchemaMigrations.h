#pragma once

namespace projection::server::db {

class SqliteConnection;

class SchemaMigrations {
public:
    static void applyMigrations(SqliteConnection& connection);
};

}  // namespace projection::server::db
