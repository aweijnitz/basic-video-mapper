#include "catch2/catch_test_macros.hpp"

int main() {
  return simple_catch::TestRegistry::instance().runAll();
}
