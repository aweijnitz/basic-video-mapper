#pragma once

#include <cstddef>
#include <vector>

namespace projection::renderer {

float mapMidiValueToBrightness(int value);

float computeAverageEnergy(const std::vector<float>& magnitudes, size_t binCount = 32);

float mapEnergyToScale(float energy, float minScale = 0.8f, float maxScale = 1.2f, float energyForMax = 1.0f);

}  // namespace projection::renderer

