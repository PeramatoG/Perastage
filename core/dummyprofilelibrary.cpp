#include "dummyprofilelibrary.h"

#include "json.hpp"
#include "projectutils.h"
#include "support.h"

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

namespace DummyProfileLibrary {

std::vector<DummyHoistProfile> LoadProfiles() {
  std::vector<DummyHoistProfile> profiles;
  const std::string baseDir = ProjectUtils::GetDefaultLibraryPath("hoists");
  if (baseDir.empty())
    return profiles;

  const fs::path filePath = fs::u8path(baseDir) / "dummy_profiles.json";
  if (!fs::exists(filePath))
    return profiles;

  std::ifstream input(filePath);
  if (!input.is_open())
    return profiles;

  json root;
  input >> root;
  if (!root.is_array())
    return profiles;

  for (const auto &entry : root) {
    if (!entry.is_object())
      continue;

    DummyHoistProfile profile;
    profile.id = entry.value("id", "");
    profile.displayName = entry.value("displayName", "");
    profile.motorName = entry.value("motorName", "");
    profile.motorManufacturer = entry.value("motorManufacturer", "");
    profile.motorModel = entry.value("motorModel", "");
    profile.capacityKg = ReadFloat(entry, "capacityKg");
    profile.weightKg = ReadFloat(entry, "weightKg");
    profile.hoistFunction =
        NormalizeHoistFunction(entry.value("hoistFunction", "Lighting"));

    if (!profile.id.empty() && !profile.displayName.empty())
      profiles.push_back(profile);
  }

  return profiles;
}

std::optional<DummyHoistProfile> FindById(const std::string &profileId) {
  if (profileId.empty())
    return std::nullopt;
  const auto profiles = LoadProfiles();
  for (const auto &profile : profiles) {
    if (profile.id == profileId)
      return profile;
  }
  return std::nullopt;
}

std::optional<DummyHoistProfile> FindByDisplayName(const std::string &displayName) {
  if (displayName.empty())
    return std::nullopt;
  const auto profiles = LoadProfiles();
  for (const auto &profile : profiles) {
    if (profile.displayName == displayName)
      return profile;
  }
  return std::nullopt;
}

} // namespace DummyProfileLibrary
