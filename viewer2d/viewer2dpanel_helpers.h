#pragma once

#include "viewer2dcommandrenderer.h"
#include <cstddef>
#include <string>

// Builds a fixture-command debug summary for a single fixture key.
std::string BuildFixtureDebugReport(const CommandBuffer &buffer,
                                    const std::string &debugKey);

// Formats a millimeter distance value as a fixed-precision meter string.
std::string FormatMeters(float millimeters);

// Computes a stable combined hash value for an ordered container of strings.
template <typename StringContainer>
size_t HashStringContainer(const StringContainer &items) {
  size_t hash = 0;
  for (const auto &item : items) {
    const size_t itemHash = std::hash<std::string>{}(item);
    hash ^= itemHash + 0x9e3779b9 + (hash << 6) + (hash >> 2);
  }
  return hash;
}
