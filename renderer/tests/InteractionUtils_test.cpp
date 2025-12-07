#include "util/InteractionUtils.h"

#include <catch2/catch_test_macros.hpp>
#include <cmath>

using projection::renderer::computeAverageEnergy;
using projection::renderer::mapEnergyToScale;
using projection::renderer::mapMidiValueToBrightness;

TEST_CASE("mapMidiValueToBrightness maps CC values to unit range", "[interaction]") {
  auto close = [](float value, float expected) { REQUIRE(std::abs(value - expected) < 1e-5f); };

  close(mapMidiValueToBrightness(0), 0.0f);
  close(mapMidiValueToBrightness(64), 64.0f / 127.0f);
  close(mapMidiValueToBrightness(127), 1.0f);
  close(mapMidiValueToBrightness(200), 1.0f);
  close(mapMidiValueToBrightness(-10), 0.0f);
}

TEST_CASE("computeAverageEnergy averages the requested number of bins", "[interaction]") {
  std::vector<float> magnitudes{1.0f, 3.0f, 5.0f, 7.0f};
  REQUIRE(std::abs(computeAverageEnergy(magnitudes, 2) - 2.0f) < 1e-5f);
  REQUIRE(std::abs(computeAverageEnergy(magnitudes, 10) - 4.0f) < 1e-5f);
  REQUIRE(std::abs(computeAverageEnergy({}, 4) - 0.0f) < 1e-5f);
}

TEST_CASE("mapEnergyToScale clamps to configured range", "[interaction]") {
  const float minScale = 0.8f;
  const float maxScale = 1.2f;
  const float energyForMax = 2.0f;

  REQUIRE(std::abs(mapEnergyToScale(0.0f, minScale, maxScale, energyForMax) - minScale) < 1e-5f);
  REQUIRE(std::abs(mapEnergyToScale(1.0f, minScale, maxScale, energyForMax) - 1.0f) < 1e-5f);
  REQUIRE(std::abs(mapEnergyToScale(5.0f, minScale, maxScale, energyForMax) - maxScale) < 1e-5f);
}

