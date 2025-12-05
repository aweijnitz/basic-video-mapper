#include "db/SchemaMigrations.h"
#include "db/SqliteConnection.h"

#include <iostream>
#include <string>

using projection::server::db::SchemaMigrations;
using projection::server::db::SqliteConnection;

int main(int argc, char** argv) {
    std::string dbPath = projection::server::db::kDefaultDbPath;
    if (argc > 1) {
        dbPath = argv[1];
    }

    try {
        SqliteConnection connection(dbPath);
        connection.open();
        SchemaMigrations migrations;
        migrations.apply(connection);
        std::cout << "SQLite database ready at " << dbPath << std::endl;
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Failed to initialize database: " << ex.what() << std::endl;
        return 1;
    }
}
