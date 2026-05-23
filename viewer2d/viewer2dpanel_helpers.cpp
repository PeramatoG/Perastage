#include "viewer2dpanel_helpers.h"

#include <iomanip>
#include <map>
#include <sstream>
#include <variant>

// Builds a histogram-style debug report for captured polygon and rectangle commands.
std::string BuildFixtureDebugReport(const CommandBuffer &buffer,
                                    const std::string &debugKey) {
  if (debugKey.empty())
    return {};

  size_t polygonCount = 0;
  size_t filledPolygons = 0;
  size_t strokedPolygons = 0;
  std::map<int, size_t> histogram;

  auto getMeta = [&](size_t idx) -> CommandMetadata {
    if (idx < buffer.metadata.size())
      return buffer.metadata[idx];
    return {};
  };

  for (size_t i = 0; i < buffer.commands.size(); ++i) {
    if (i >= buffer.sources.size())
      break;
    if (buffer.sources[i] != debugKey)
      continue;

    const auto &cmd = buffer.commands[i];
    const auto meta = getMeta(i);
    auto addEntry = [&](int vertices) {
      ++polygonCount;
      ++histogram[vertices];
      if (meta.hasFill)
        ++filledPolygons;
      if (meta.hasStroke)
        ++strokedPolygons;
    };

    if (std::holds_alternative<PolygonCommand>(cmd)) {
      const auto &poly = std::get<PolygonCommand>(cmd);
      addEntry(static_cast<int>(poly.points.size() / 2));
    } else if (std::holds_alternative<RectangleCommand>(cmd)) {
      addEntry(4);
    }
  }

  if (polygonCount == 0)
    return {};

  std::ostringstream out;
  out << "Fixture capture debug ['" << debugKey << "']: polygons="
      << polygonCount << ", filled=" << filledPolygons
      << ", stroked=" << strokedPolygons << "\nVertex histogram:";
  for (const auto &[verts, count] : histogram)
    out << ' ' << verts << "->" << count;

  return out.str();
}

// Converts a millimeter value into a three-decimal meter string representation.
std::string FormatMeters(float millimeters) {
  std::ostringstream out;
  out << std::fixed << std::setprecision(3)
      << (static_cast<double>(millimeters) / 1000.0);
  return out.str();
}
