/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */

#include "print_diagnostics.h"

#include <algorithm>
#include <iomanip>
#include <map>
#include <numeric>
#include <sstream>
#include <type_traits>
#include <unordered_map>

namespace {

std::string FormatFloat(double v) {
  std::ostringstream ss;
  ss << std::fixed << std::setprecision(3) << v;
  return ss.str();
}

void AppendStrokeEstimate(std::ostringstream &out, const CanvasStroke &stroke) {
  out << FormatFloat(stroke.color.r) << ' ' << FormatFloat(stroke.color.g) << ' '
      << FormatFloat(stroke.color.b) << " RG\n";
  out << FormatFloat(stroke.width) << " w\n";
}

void AppendFillEstimate(std::ostringstream &out, const CanvasFill &fill) {
  out << FormatFloat(fill.color.r) << ' ' << FormatFloat(fill.color.g) << ' '
      << FormatFloat(fill.color.b) << " rg\n";
}

size_t EstimateLineBytes(const LineCommand &cmd) {
  std::ostringstream out;
  AppendStrokeEstimate(out, cmd.stroke);
  out << FormatFloat(cmd.x0) << ' ' << FormatFloat(cmd.y0) << " m\n"
      << FormatFloat(cmd.x1) << ' ' << FormatFloat(cmd.y1) << " l\nS\n";
  return out.str().size();
}

size_t EstimatePolylineBytes(const PolylineCommand &cmd) {
  if (cmd.points.size() < 4)
    return 0;
  std::ostringstream out;
  AppendStrokeEstimate(out, cmd.stroke);
  out << FormatFloat(cmd.points[0]) << ' ' << FormatFloat(cmd.points[1]) << " m\n";
  for (size_t i = 2; i + 1 < cmd.points.size(); i += 2) {
    out << FormatFloat(cmd.points[i]) << ' ' << FormatFloat(cmd.points[i + 1])
        << " l\n";
  }
  out << "S\n";
  return out.str().size();
}

size_t EstimatePolygonBytes(const PolygonCommand &cmd) {
  if (cmd.points.size() < 6)
    return 0;
  std::ostringstream out;
  if (cmd.hasFill)
    AppendFillEstimate(out, cmd.fill);
  out << FormatFloat(cmd.points[0]) << ' ' << FormatFloat(cmd.points[1]) << " m\n";
  for (size_t i = 2; i + 1 < cmd.points.size(); i += 2)
    out << FormatFloat(cmd.points[i]) << ' ' << FormatFloat(cmd.points[i + 1])
        << " l\n";
  out << "h\n";
  if (cmd.hasFill && cmd.stroke.width > 0.0f) {
    AppendStrokeEstimate(out, cmd.stroke);
    out << "B\n";
  } else if (cmd.hasFill) {
    out << "f\n";
  } else {
    AppendStrokeEstimate(out, cmd.stroke);
    out << "S\n";
  }
  return out.str().size();
}

size_t EstimateRectangleBytes(const RectangleCommand &cmd) {
  std::ostringstream out;
  if (cmd.hasFill)
    AppendFillEstimate(out, cmd.fill);
  out << FormatFloat(cmd.x) << ' ' << FormatFloat(cmd.y) << ' '
      << FormatFloat(cmd.w) << ' ' << FormatFloat(cmd.h) << " re\n";
  if (cmd.hasFill && cmd.stroke.width > 0.0f) {
    AppendStrokeEstimate(out, cmd.stroke);
    out << "B\n";
  } else if (cmd.hasFill) {
    out << "f\n";
  } else {
    AppendStrokeEstimate(out, cmd.stroke);
    out << "S\n";
  }
  return out.str().size();
}

size_t EstimateCircleBytes(const CircleCommand &cmd) {
  // Circle approximation uses 4 Bezier segments; keep the estimate simple.
  std::ostringstream out;
  if (cmd.hasFill)
    AppendFillEstimate(out, cmd.fill);
  out << FormatFloat(cmd.cx) << ' ' << FormatFloat(cmd.cy) << " m\n";
  if (cmd.hasFill && cmd.stroke.width > 0.0f) {
    AppendStrokeEstimate(out, cmd.stroke);
    out << "B\n";
  } else if (cmd.hasFill) {
    out << "f\n";
  } else {
    AppendStrokeEstimate(out, cmd.stroke);
    out << "S\n";
  }
  return out.str().size();
}

size_t EstimateTextBytes(const TextCommand &cmd) {
  std::ostringstream out;
  out << "BT\n/F1 " << FormatFloat(cmd.style.fontSize) << " Tf\n";
  out << FormatFloat(cmd.style.color.r) << ' ' << FormatFloat(cmd.style.color.g)
      << ' ' << FormatFloat(cmd.style.color.b) << " rg\n";
  out << FormatFloat(cmd.x) << ' ' << FormatFloat(cmd.y) << " Td\n("
      << cmd.text << ") Tj\nET\n";
  return out.str().size();
}

std::string CommandName(const CanvasCommand &cmd) {
  return std::visit(
      [](auto &&value) -> std::string {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, LineCommand>)
          return "Line";
        else if constexpr (std::is_same_v<T, PolylineCommand>)
          return "Polyline";
        else if constexpr (std::is_same_v<T, PolygonCommand>)
          return "Polygon";
        else if constexpr (std::is_same_v<T, RectangleCommand>)
          return "Rectangle";
        else if constexpr (std::is_same_v<T, CircleCommand>)
          return "Circle";
        else if constexpr (std::is_same_v<T, TextCommand>)
          return "Text";
        else if constexpr (std::is_same_v<T, SaveCommand>)
          return "Save";
        else if constexpr (std::is_same_v<T, RestoreCommand>)
          return "Restore";
        else if constexpr (std::is_same_v<T, TransformCommand>)
          return "Transform";
        else
          return "Unknown";
      },
      cmd);
}

size_t EstimateBytes(const CanvasCommand &cmd) {
  return std::visit(
      [](auto &&value) -> size_t {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, LineCommand>)
          return EstimateLineBytes(value);
        else if constexpr (std::is_same_v<T, PolylineCommand>)
          return EstimatePolylineBytes(value);
        else if constexpr (std::is_same_v<T, PolygonCommand>)
          return EstimatePolygonBytes(value);
        else if constexpr (std::is_same_v<T, RectangleCommand>)
          return EstimateRectangleBytes(value);
        else if constexpr (std::is_same_v<T, CircleCommand>)
          return EstimateCircleBytes(value);
        else if constexpr (std::is_same_v<T, TextCommand>)
          return EstimateTextBytes(value);
        else
          return static_cast<size_t>(0);
      },
      cmd);
}

} // namespace

std::string BuildPrintDiagnostics(const CommandBuffer &buffer,
                                  size_t topTypeCount) {
  std::unordered_map<std::string, size_t> commandCounts;
  std::map<int, size_t> polygonHistogram;
  std::unordered_map<std::string, std::pair<size_t, size_t>> typePolygonStats;
  size_t estimatedBytes = 0;

  for (size_t idx = 0; idx < buffer.commands.size(); ++idx) {
    const auto &cmd = buffer.commands[idx];
    std::string name = CommandName(cmd);
    ++commandCounts[name];
    estimatedBytes += EstimateBytes(cmd);

    auto typeKey = (idx < buffer.sources.size() && !buffer.sources[idx].empty())
                       ? buffer.sources[idx]
                       : std::string("unknown");

    if (std::holds_alternative<PolygonCommand>(cmd)) {
      const auto &poly = std::get<PolygonCommand>(cmd);
      int verts = static_cast<int>(poly.points.size() / 2);
      ++polygonHistogram[verts];
      auto &entry = typePolygonStats[typeKey];
      entry.first += 1;
      entry.second += static_cast<size_t>(verts);
    } else if (std::holds_alternative<RectangleCommand>(cmd)) {
      ++polygonHistogram[4];
      auto &entry = typePolygonStats[typeKey];
      entry.first += 1;
      entry.second += 4;
    }
  }

  std::vector<std::pair<std::string, std::pair<size_t, size_t>>> typeStats(
      typePolygonStats.begin(), typePolygonStats.end());
  std::sort(typeStats.begin(), typeStats.end(),
            [](const auto &a, const auto &b) {
              return a.second.second > b.second.second;
            });

  size_t triangleCount = 0;
  size_t quadCount = 0;
  size_t complexCount = 0;
  for (const auto &[verts, count] : polygonHistogram) {
    if (verts == 3)
      triangleCount += count;
    else if (verts == 4)
      quadCount += count;
    else if (verts >= 5)
      complexCount += count;
  }

  std::ostringstream report;
  report << "Print Plan diagnostics\n";
  report << "Total commands: " << buffer.commands.size() << "\n";
  report << "Command counts:\n";
  for (const auto &[name, count] : commandCounts)
    report << "  " << name << ": " << count << "\n";

  report << "Polygon histogram:\n";
  report << "  Triangles: " << triangleCount << "\n";
  report << "  Quads: " << quadCount << "\n";
  report << "  5+ verts: " << complexCount << "\n";

  report << "Top polygon contributors:\n";
  size_t printed = 0;
  for (const auto &entry : typeStats) {
    if (printed++ >= topTypeCount)
      break;
    report << "  " << entry.first << ": " << entry.second.first
           << " polygons / " << entry.second.second << " verts\n";
  }
  if (typeStats.empty())
    report << "  (no polygon data)\n";

  report << "Estimated content bytes: " << estimatedBytes << "\n";
  return report.str();
}

