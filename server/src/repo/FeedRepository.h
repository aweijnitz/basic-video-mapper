#pragma once

#include <vector>
#include <optional>

#include "db/SqliteConnection.h"
#include "projection/core/Feed.h"

namespace projection::server::repo {

// Repository responsible for persisting and retrieving Feed objects.
// All functions throw std::runtime_error on SQL errors or invalid data.
class FeedRepository {
public:
    explicit FeedRepository(db::SqliteConnection& connection);

    core::Feed createFeed(const core::Feed& feed);
    std::vector<core::Feed> listFeeds();
    std::optional<core::Feed> findFeedById(const core::FeedId& id);
    core::Feed updateFeed(const core::Feed& feed);
    void deleteFeed(const core::FeedId& id);

private:
    db::SqliteConnection& connection_;
};

}  // namespace projection::server::repo
