#pragma once

#include <optional>
#include <string>
#include <vector>

struct DummyHoistProfile {
  std::string id;
  std::string displayName;
  std::string motorName;
  std::string motorManufacturer;
  std::string motorModel;
  float capacityKg = 0.0f;
  float weightKg = 0.0f;
  std::string hoistFunction = "Lighting";
};

namespace DummyProfileLibrary {
std::vector<DummyHoistProfile> LoadProfiles();
std::optional<DummyHoistProfile> FindById(const std::string &profileId);
std::optional<DummyHoistProfile> FindByDisplayName(const std::string &displayName);
} // namespace DummyProfileLibrary
