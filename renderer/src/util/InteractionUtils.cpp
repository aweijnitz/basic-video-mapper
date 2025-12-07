#include "util/InteractionUtils.h"

#include <algorithm>
#include <cmath>

namespace projection::renderer {

float mapMidiValueToBrightness(int value) {
  const int clampedValue = std::clamp(value, 0, 127);
  return static_cast<float>(clampedValue) / 127.0f;
}

float computeAverageEnergy(const std::vector<float>& magnitudes, size_t binCount) {
  if (magnitudes.empty() || binCount == 0) {
    return 0.0f;
  }

  const size_t count = std::min(binCount, magnitudes.size());
  float sum = 0.0f;
  for (size_t i = 0; i < count; ++i) {
    sum += std::abs(magnitudes[i]);
  }
  return sum / static_cast<float>(count);
}

float mapEnergyToScale(float energy, float minScale, float maxScale, float energyForMax) {
  if (energyForMax <= 0.0f) {
    return minScale;
  }

  const float normalized = std::clamp(energy / energyForMax, 0.0f, 1.0f);
  return minScale + normalized * (maxScale - minScale);
}

}  // namespace projection::renderer

