#pragma once

#include "db/SqliteConnection.h"

namespace projection::server::db {

class SchemaMigrations {
public:
    static constexpr int kCurrentSchemaVersion = 1;

    explicit SchemaMigrations(int targetVersion = kCurrentSchemaVersion);

    void apply(SqliteConnection& connection);

private:
    int readCurrentVersion(SqliteConnection& connection);
    void applyInitialMigration(SqliteConnection& connection);

    int targetVersion;
};

}  // namespace projection::server::db
