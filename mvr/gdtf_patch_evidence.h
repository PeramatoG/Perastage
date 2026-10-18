/* This file is part of Perastage, licensed under GPL-3.0-or-later. */
#pragma once
#include <string>
#include <unordered_map>
#include <vector>

namespace mvr::gdtf_patch_evidence {
struct Entry {
  std::string type;
  std::string mode;
  std::string address;
};
// Repeated spacing can resolve a unique mode when authored mode/footprint data
// is absent. It never proves DMX channel semantics or overrides authored data.
std::unordered_map<std::string, int> RepeatedSpacing(const std::vector<Entry> &patch);
}
