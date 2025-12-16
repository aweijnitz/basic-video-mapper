#include "repo/FeedRepository.h"

#include <sqlite3.h>

#include <chrono>
#include <stdexcept>
#include <string>

#include "projection/core/Enums.h"

namespace projection::server::repo {

namespace {
std::string generateId() {
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    return "feed-" + std::to_string(now);
}
}

FeedRepository::FeedRepository(db::SqliteConnection& connection) : connection_(connection) {}

core::Feed FeedRepository::createFeed(const core::Feed& feed) {
    sqlite3* handle = connection_.getHandle();
    if (handle == nullptr) {
        throw std::runtime_error("SQLite connection is not open");
    }

    const std::string idValue = feed.getId().value.empty() ? generateId() : feed.getId().value;
    const char* sql = "INSERT INTO feeds(id, name, type, config_json) VALUES(?, ?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;
    int result = sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare feed insert statement: " + std::string(sqlite3_errmsg(handle)));
    }

    int bindIndex = 1;
    result = sqlite3_bind_text(stmt, bindIndex++, idValue.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind feed id: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_bind_text(stmt, bindIndex++, feed.getName().c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind feed name: " + std::string(sqlite3_errmsg(handle)));
    }
    std::string typeString = core::toString(feed.getType());
    result = sqlite3_bind_text(stmt, bindIndex++, typeString.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind feed type: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_bind_text(stmt, bindIndex, feed.getConfigJson().c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind feed config: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_step(stmt);
    if (result != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to insert feed: " + std::string(sqlite3_errmsg(handle)));
    }

    sqlite3_finalize(stmt);

    core::Feed created = feed;
    created.setId(core::FeedId(idValue));
    return created;
}

std::vector<core::Feed> FeedRepository::listFeeds() {
    sqlite3* handle = connection_.getHandle();
    if (handle == nullptr) {
        throw std::runtime_error("SQLite connection is not open");
    }

    const char* sql = "SELECT id, name, type, config_json FROM feeds ORDER BY id;";

    sqlite3_stmt* stmt = nullptr;
    int result = sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare feed select statement: " + std::string(sqlite3_errmsg(handle)));
    }

    std::vector<core::Feed> feeds;
    while ((result = sqlite3_step(stmt)) == SQLITE_ROW) {
        const unsigned char* idText = sqlite3_column_text(stmt, 0);
        const unsigned char* nameText = sqlite3_column_text(stmt, 1);
        const unsigned char* typeText = sqlite3_column_text(stmt, 2);
        const unsigned char* configText = sqlite3_column_text(stmt, 3);

        std::string idString = idText ? reinterpret_cast<const char*>(idText) : "";
        std::string name = nameText ? reinterpret_cast<const char*>(nameText) : "";
        std::string typeStr = typeText ? reinterpret_cast<const char*>(typeText) : "";
        std::string configJson = configText ? reinterpret_cast<const char*>(configText) : "";

        core::FeedType feedType;
        if (!core::fromString(typeStr, feedType)) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("Failed to parse feed type from database: " + typeStr);
        }

        feeds.emplace_back(core::Feed(core::FeedId(idString), name, feedType, configJson));
    }

    if (result != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to read feeds: " + std::string(sqlite3_errmsg(handle)));
    }

    sqlite3_finalize(stmt);
    return feeds;
}

std::optional<core::Feed> FeedRepository::findFeedById(const core::FeedId& feedId) {
    sqlite3* handle = connection_.getHandle();
    if (handle == nullptr) {
        throw std::runtime_error("SQLite connection is not open");
    }

    const char* sql = "SELECT id, name, type, config_json FROM feeds WHERE id=? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    int result = sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare feed select statement: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_bind_text(stmt, 1, feedId.value.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind feed id: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_step(stmt);
    if (result == SQLITE_ROW) {
        const unsigned char* idText = sqlite3_column_text(stmt, 0);
        const unsigned char* nameText = sqlite3_column_text(stmt, 1);
        const unsigned char* typeText = sqlite3_column_text(stmt, 2);
        const unsigned char* configText = sqlite3_column_text(stmt, 3);

        std::string idString = idText ? reinterpret_cast<const char*>(idText) : "";
        std::string name = nameText ? reinterpret_cast<const char*>(nameText) : "";
        std::string typeStr = typeText ? reinterpret_cast<const char*>(typeText) : "";
        std::string configJson = configText ? reinterpret_cast<const char*>(configText) : "";

        core::FeedType feedType;
        if (!core::fromString(typeStr, feedType)) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("Failed to parse feed type from database: " + typeStr);
        }

        sqlite3_finalize(stmt);
        return core::Feed(core::FeedId(idString), name, feedType, configJson);
    }

    sqlite3_finalize(stmt);
    return std::nullopt;
}

core::Feed FeedRepository::updateFeed(const core::Feed& feed) {
    sqlite3* handle = connection_.getHandle();
    if (handle == nullptr) {
        throw std::runtime_error("SQLite connection is not open");
    }
    if (feed.getId().value.empty()) {
        throw std::runtime_error("Feed id must not be empty for update");
    }

    const char* sql = "UPDATE feeds SET name=?, type=?, config_json=? WHERE id=?;";
    sqlite3_stmt* stmt = nullptr;
    int result = sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare feed update statement: " + std::string(sqlite3_errmsg(handle)));
    }

    std::string typeString = core::toString(feed.getType());
    result = sqlite3_bind_text(stmt, 1, feed.getName().c_str(), -1, SQLITE_TRANSIENT);
    result |= sqlite3_bind_text(stmt, 2, typeString.c_str(), -1, SQLITE_TRANSIENT);
    result |= sqlite3_bind_text(stmt, 3, feed.getConfigJson().c_str(), -1, SQLITE_TRANSIENT);
    result |= sqlite3_bind_text(stmt, 4, feed.getId().value.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind feed update fields: " + std::string(sqlite3_errmsg(handle)));
    }

    result = sqlite3_step(stmt);
    if (result != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to update feed: " + std::string(sqlite3_errmsg(handle)));
    }
    sqlite3_finalize(stmt);
    return feed;
}

void FeedRepository::deleteFeed(const core::FeedId& id) {
    sqlite3* handle = connection_.getHandle();
    if (handle == nullptr) {
        throw std::runtime_error("SQLite connection is not open");
    }
    const char* sql = "DELETE FROM feeds WHERE id=?;";
    sqlite3_stmt* stmt = nullptr;
    int result = sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare feed delete statement: " + std::string(sqlite3_errmsg(handle)));
    }
    result = sqlite3_bind_text(stmt, 1, id.value.c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to bind feed id for delete: " + std::string(sqlite3_errmsg(handle)));
    }
    result = sqlite3_step(stmt);
    if (result != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to delete feed: " + std::string(sqlite3_errmsg(handle)));
    }
    sqlite3_finalize(stmt);
}

}  // namespace projection::server::repo
