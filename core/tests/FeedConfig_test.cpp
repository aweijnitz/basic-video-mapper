#include "projection/core/Feed.h"

#include <catch2/catch_test_macros.hpp>
#include <stdexcept>

using projection::core::Feed;
using projection::core::FeedId;
using projection::core::FeedType;
using projection::core::VideoFileConfig;
using projection::core::makeVideoFileFeed;
using projection::core::parseVideoFileConfig;

TEST_CASE("makeVideoFileFeed round-trips file path", "[core][feed][config]") {
    const std::string filePath = "/videos/demo.mp4";
    Feed feed = makeVideoFileFeed(FeedId{"10"}, "Demo", filePath);

    REQUIRE(feed.getType() == FeedType::VideoFile);

    VideoFileConfig config = parseVideoFileConfig(feed);
    REQUIRE(config.filePath == filePath);
}

TEST_CASE("parseVideoFileConfig rejects non-video feeds", "[core][feed][config][error]") {
    Feed feed(FeedId{"11"}, "Camera", FeedType::Camera, "{}");
    bool threw = false;
    try {
        parseVideoFileConfig(feed);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    REQUIRE(threw);
}

TEST_CASE("parseVideoFileConfig validates config JSON", "[core][feed][config][error]") {
    Feed feed(FeedId{"12"}, "Video", FeedType::VideoFile, "{\"wrong\":true}");
    bool threw = false;
    try {
        parseVideoFileConfig(feed);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    REQUIRE(threw);
}
