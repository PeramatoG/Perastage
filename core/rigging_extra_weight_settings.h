#pragma once

#include <optional>
#include <string>
#include <unordered_map>

namespace RiggingExtraWeightSettings {

struct Entry {
  float valueKg = 0.0f;
  bool requiresValidation = false;
};

using EntriesByPosition = std::unordered_map<std::string, Entry>;
using KilogramsByPosition = std::unordered_map<std::string, float>;

constexpr const char *ConfigKey() { return "rigging_position_extra_weights_v1"; }

EntriesByPosition ParseEntries(const std::optional<std::string> &serialized);
std::string SerializeEntries(const EntriesByPosition &entries);
KilogramsByPosition BuildKilogramsByPosition(const EntriesByPosition &entries);

} // namespace RiggingExtraWeightSettings
