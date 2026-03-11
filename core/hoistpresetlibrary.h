#pragma once

#include <optional>
#include <string>
#include <vector>

struct HoistPreset {
  std::string id;
  std::string name;
  std::string motorName;
  std::string motorManufacturer;
  std::string motorModel;
  std::string dummyPreset;
  float capacityKg = 0.0f;
  float weightKg = 0.0f;
  std::string hoistFunction = "Lighting";
};

namespace HoistPresetLibrary {
std::vector<HoistPreset> LoadPresets();
std::optional<HoistPreset> FindByDummyPreset(const std::string &dummyPreset);
} // namespace HoistPresetLibrary
