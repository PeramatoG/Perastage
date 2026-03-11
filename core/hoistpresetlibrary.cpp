#include "hoistpresetlibrary.h"

#include "projectutils.h"
#include "support.h"
#include "json.hpp"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {

float ReadFloat(const json &obj, const char *key) {
  auto it = obj.find(key);
  if (it == obj.end())
    return 0.0f;
  if (it->is_number_float())
    return it->get<float>();
  if (it->is_number_integer())
    return static_cast<float>(it->get<int>());
  return 0.0f;
}

} // namespace

namespace HoistPresetLibrary {

std::vector<HoistPreset> LoadPresets() {
  std::vector<HoistPreset> presets;
  const std::string baseDir = ProjectUtils::GetDefaultLibraryPath("hoists");
  if (baseDir.empty())
    return presets;

  const fs::path filePath = fs::u8path(baseDir) / "hoist_presets.json";
  if (!fs::exists(filePath))
    return presets;

  std::ifstream input(filePath);
  if (!input.is_open())
    return presets;

  json root;
  input >> root;
  if (!root.is_array())
    return presets;

  for (const auto &entry : root) {
    if (!entry.is_object())
      continue;

    HoistPreset preset;
    preset.id = entry.value("id", "");
    preset.name = entry.value("name", "");
    preset.motorName = entry.value("motorName", "");
    preset.dummyPreset = entry.value("dummyPreset", "");
    preset.capacityKg = ReadFloat(entry, "capacityKg");
    preset.weightKg = ReadFloat(entry, "weightKg");
    preset.hoistFunction = NormalizeHoistFunction(entry.value("hoistFunction", "Lighting"));
    if (!preset.dummyPreset.empty())
      presets.push_back(preset);
  }

  return presets;
}

std::optional<HoistPreset> FindByDummyPreset(const std::string &dummyPreset) {
  const auto presets = LoadPresets();
  for (const auto &preset : presets) {
    if (preset.dummyPreset == dummyPreset)
      return preset;
  }
  return std::nullopt;
}

} // namespace HoistPresetLibrary
