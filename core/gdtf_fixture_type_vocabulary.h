#pragma once

#include <algorithm>
#include <array>
#include <string_view>

namespace gdtf {

inline constexpr std::array<std::string_view, 9> kFixtureTypeChildOrder = {
    "AttributeDefinitions",
    "Wheels",
    "PhysicalDescriptions",
    "Models",
    "Geometries",
    "DMXModes",
    "Revisions",
    "FTPresets",
    "Protocols"};

inline constexpr std::array<std::string_view, 11> kFixtureTypeAttributes = {
    "Name",         "ShortName",        "LongName",
    "Manufacturer", "Description",      "FixtureTypeID",
    "Thumbnail",    "ThumbnailOffsetX", "ThumbnailOffsetY",
    "RefFT",        "CanHaveChildren"};

// Reports whether a name is a standard direct FixtureType child in GDTF 1.2.
inline bool IsStandardFixtureTypeChild(std::string_view name) {
  return std::find(kFixtureTypeChildOrder.begin(), kFixtureTypeChildOrder.end(),
                   name) != kFixtureTypeChildOrder.end();
}

// Returns the GDTF 1.2 order index for a direct FixtureType child.
inline int FixtureTypeChildOrderIndex(std::string_view name) {
  const auto found = std::find(kFixtureTypeChildOrder.begin(),
                               kFixtureTypeChildOrder.end(), name);
  return found == kFixtureTypeChildOrder.end()
             ? -1
             : static_cast<int>(found - kFixtureTypeChildOrder.begin());
}

// Reports whether a name is a standard FixtureType attribute in GDTF 1.2.
inline bool IsStandardFixtureTypeAttribute(std::string_view name) {
  return std::find(kFixtureTypeAttributes.begin(), kFixtureTypeAttributes.end(),
                   name) != kFixtureTypeAttributes.end();
}

// Recognizes the legacy Perastage-owned Editor marker without matching vendors.
inline bool IsLegacyPerastageEditorValue(std::string_view value) {
  return value == "Perastage" || value.rfind("Perastage ", 0) == 0;
}

} // namespace gdtf
