/* This file is part of Perastage, licensed under GPL-3.0-or-later. */
#include "gdtf_patch_evidence.h"
#include <algorithm>
#include <charconv>
#include <map>
#include <set>

namespace mvr::gdtf_patch_evidence {
namespace {
int PositiveInteger(const std::string &text) {
  int value = 0;
  const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
  return error == std::errc{} && end == text.data() + text.size() && value > 0 ? value : 0;
}
struct Observation {
  int intervals = 0;
  std::set<int> gaps;
  std::set<std::string> modes;
  bool invalid = false;
};
}
std::unordered_map<std::string, int> RepeatedSpacing(const std::vector<Entry> &patch) {
  std::map<int, std::map<int, std::vector<const Entry *>>> universes;
  std::unordered_map<std::string, Observation> evidence;
  for (const auto &entry : patch) {
    auto &observation = evidence[entry.type];
    observation.modes.insert(entry.mode);
    const auto dot = entry.address.find('.');
    const int universe = dot == std::string::npos ? 0 : PositiveInteger(entry.address.substr(0, dot));
    const int address = dot == std::string::npos ? 0 : PositiveInteger(entry.address.substr(dot + 1));
    if (universe <= 0 || address <= 0 || address > 512) {
      observation.invalid = true;
      continue;
    }
    universes[universe][address].push_back(&entry);
  }
  for (const auto &[universe, addresses] : universes) {
    (void)universe;
    const Entry *previous = nullptr;
    int previousAddress = 0;
    for (const auto &[address, entries] : addresses) {
      if (entries.size() != 1) {
        for (const auto *entry : entries)
          evidence[entry->type].invalid = true;
        previous = nullptr;
        continue;
      }
      const auto *entry = entries.front();
      if (previous && previous->type == entry->type && previous->mode == entry->mode) {
        auto &observation = evidence[entry->type];
        ++observation.intervals;
        observation.gaps.insert(address - previousAddress);
      }
      previous = entry;
      previousAddress = address;
    }
  }
  std::unordered_map<std::string, int> hints;
  for (const auto &[type, observation] : evidence)
    if (!observation.invalid && observation.intervals >= 2 &&
        observation.modes.size() == 1 && observation.gaps.size() == 1)
      hints[type] = *observation.gaps.begin();
  return hints;
}
}
