#include "rigging_extra_weight_settings.h"

#include <map>

#include "json.hpp"

namespace RiggingExtraWeightSettings {

EntriesByPosition ParseEntries(const std::optional<std::string> &serialized) {
  EntriesByPosition result;
  if (!serialized || serialized->empty())
    return result;

  const nlohmann::json parsed = nlohmann::json::parse(*serialized, nullptr, false);
  if (!parsed.is_object())
    return result;

  for (auto it = parsed.begin(); it != parsed.end(); ++it) {
    if (!it.value().is_object())
      continue;

    Entry entry;
    if (const auto valueIt = it.value().find("valueKg");
        valueIt != it.value().end() && valueIt->is_number()) {
      entry.valueKg = static_cast<float>(valueIt->get<double>());
    }
    if (const auto sourceIt = it.value().find("source");
        sourceIt != it.value().end() && sourceIt->is_string() &&
        sourceIt->get<std::string>() == "auto_unvalidated") {
      entry.requiresValidation = true;
    }

    result[it.key()] = entry;
  }

  return result;
}

std::string SerializeEntries(const EntriesByPosition &entries) {
  nlohmann::json root = nlohmann::json::object();
  std::map<std::string, Entry> sortedEntries(entries.begin(), entries.end());
  for (const auto &[position, entry] : sortedEntries) {
    nlohmann::json item = nlohmann::json::object();
    item["valueKg"] = entry.valueKg;
    item["source"] = entry.requiresValidation ? "auto_unvalidated" : "manual";
    root[position] = std::move(item);
  }
  return root.dump();
}

KilogramsByPosition BuildKilogramsByPosition(const EntriesByPosition &entries) {
  KilogramsByPosition kilogramsByPosition;
  kilogramsByPosition.reserve(entries.size());
  for (const auto &[position, entry] : entries) {
    kilogramsByPosition[position] = entry.valueKg;
  }
  return kilogramsByPosition;
}

} // namespace RiggingExtraWeightSettings
