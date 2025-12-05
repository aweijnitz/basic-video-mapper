#include <catch2/catch_test_macros.hpp>

#include "projection/core/CoreInit.h"

TEST_CASE("Core library initializes", "[core]") {
  REQUIRE(projection::core::coreInitialized());
}
