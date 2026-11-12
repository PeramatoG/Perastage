#pragma once

#include <map>
#include <string>
#include <vector>

namespace tinyxml2 { class XMLElement; }
namespace gdtf {
struct GdtfPhysicalChannelInstance {
  int dmxBreak = 1;
  int referenceIndex = 0;
  std::vector<int> offsets;
};
struct GdtfChannelAddressResolution {
  bool virtualChannel = false;
  std::vector<GdtfPhysicalChannelInstance> instances;
};
struct GdtfModeFootprint {
  // Source channel order, including virtual and unresolved channels.
  std::vector<GdtfChannelAddressResolution> channels;
  std::map<int, int> footprintsByBreak;
  std::vector<std::string> diagnostics;
  // The existing single-address UI represents break 1, never a sum of breaks.
  int PrimaryFootprint() const;
};
// Read-only resolution; XML elements and authored values are never modified.
GdtfModeFootprint ResolveGdtfModeFootprint(
    const tinyxml2::XMLElement* geometries, const tinyxml2::XMLElement* mode);
} // namespace gdtf
