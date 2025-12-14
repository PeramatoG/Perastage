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

#include "planpdfexporter.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <zlib.h>

namespace {

constexpr float PIXELS_PER_METER = 25.0f;
// Matches the tighter line spacing used by the on-screen 2D viewer when
// rendering multi-line text labels. Slightly below 1.0 to mirror the compact
// leading applied in the live view.
constexpr float PDF_TEXT_LINE_HEIGHT_FACTOR = 0.78f;
// Approximates the ascent of the standard Helvetica font used by PDF viewers
// (718 units over 1000). Used to align top-anchored text consistently with the
// on-screen NanoVG rendering that places text relative to its bounding box top
// edge.
constexpr float PDF_TEXT_ASCENT_FACTOR = 0.718f;
// Complements the ascent factor using Helvetica's 207 unit descent to align the
// PDF export's baseline with the on-screen text metrics.
constexpr float PDF_TEXT_DESCENT_FACTOR = 0.207f;

double ComputeTextLineAdvance(const CanvasTextStyle &style) {
  // Negative because PDF moves the text cursor downward with a negative y
  // translation. Line advance mirrors the ascent + descent used by the
  // on-screen viewer when positioning multi-line labels.
  return -style.fontSize * (PDF_TEXT_ASCENT_FACTOR + PDF_TEXT_DESCENT_FACTOR) *
         PDF_TEXT_LINE_HEIGHT_FACTOR;
}

struct PdfObject {
  std::string body;
};

class FloatFormatter {
public:
  explicit FloatFormatter(int precision)
      : precision_(std::clamp(precision, 0, 6)) {}

  std::string Format(double value) const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(precision_) << value;
    return ss.str();
  }

private:
  int precision_;
};

class PdfDeflater {
public:
  static bool Compress(const std::string &input, std::string &output,
                       std::string &error) {
    if (input.empty()) {
      output.clear();
      return true;
    }

    uLongf bound = compressBound(input.size());
    std::string compressed;
    compressed.resize(bound);

    int zres = compress2(reinterpret_cast<Bytef *>(compressed.data()), &bound,
                         reinterpret_cast<const Bytef *>(input.data()),
                         input.size(), Z_BEST_SPEED);
    if (zres != Z_OK) {
      error = "compress2 failed";
      return false;
    }

    compressed.resize(bound);
    output.swap(compressed);
    return true;
  }
};

struct Point {
  double x = 0.0;
  double y = 0.0;
};

struct Transform {
  double scale = 1.0;
  double offsetX = 0.0;
  double offsetY = 0.0;
};

struct Mapping {
  double minX = 0.0;
  double minY = 0.0;
  double scale = 1.0;
  double offsetX = 0.0;
  double offsetY = 0.0;
  double pageHeight = 0.0;
  bool flipY = true;
};

Point Apply(const Transform &t, double x, double y) {
  return {x * t.scale + t.offsetX, y * t.scale + t.offsetY};
}

Point MapWithMapping(double x, double y, const Mapping &mapping) {
  double px = mapping.offsetX + (x - mapping.minX) * mapping.scale;
  double py = mapping.offsetY + (y - mapping.minY) * mapping.scale;
  if (mapping.flipY)
    py = mapping.pageHeight - mapping.offsetY -
         (y - mapping.minY) * mapping.scale;
  return {px, py};
}

class GraphicsStateCache {
public:
  void SetStroke(std::ostringstream &out, const CanvasStroke &stroke,
                 const FloatFormatter &fmt) {
    if (!joinStyleSet_) {
      out << "1 j\n";
      joinStyleSet_ = true;
    }
    if (!capStyleSet_) {
      out << "1 J\n";
      capStyleSet_ = true;
    }
    if (!hasStrokeColor_ || !SameColor(stroke.color, strokeColor_)) {
      out << fmt.Format(stroke.color.r) << ' ' << fmt.Format(stroke.color.g) << ' '
          << fmt.Format(stroke.color.b) << " RG\n";
      strokeColor_ = stroke.color;
      hasStrokeColor_ = true;
    }
    if (!hasLineWidth_ || std::abs(stroke.width - lineWidth_) > 1e-6) {
      out << fmt.Format(stroke.width) << " w\n";
      lineWidth_ = stroke.width;
      hasLineWidth_ = true;
    }
  }

  void SetFill(std::ostringstream &out, const CanvasFill &fill,
               const FloatFormatter &fmt) {
    if (!hasFillColor_ || !SameColor(fill.color, fillColor_)) {
      out << fmt.Format(fill.color.r) << ' ' << fmt.Format(fill.color.g) << ' '
          << fmt.Format(fill.color.b) << " rg\n";
      fillColor_ = fill.color;
      hasFillColor_ = true;
    }
  }

private:
  static bool SameColor(const CanvasColor &a, const CanvasColor &b) {
    return std::abs(a.r - b.r) < 1e-6 && std::abs(a.g - b.g) < 1e-6 &&
           std::abs(a.b - b.b) < 1e-6;
  }

  CanvasColor strokeColor_{};
  CanvasColor fillColor_{};
  double lineWidth_ = -1.0;
  bool hasStrokeColor_ = false;
  bool hasFillColor_ = false;
  bool hasLineWidth_ = false;
  bool joinStyleSet_ = false;
  bool capStyleSet_ = false;
};

void AppendLine(std::ostringstream &out, GraphicsStateCache &cache,
                const FloatFormatter &fmt, const Point &a, const Point &b,
                const CanvasStroke &stroke) {
  cache.SetStroke(out, stroke, fmt);
  out << fmt.Format(a.x) << ' ' << fmt.Format(a.y) << " m\n"
      << fmt.Format(b.x) << ' ' << fmt.Format(b.y) << " l\nS\n";
}

void AppendPolyline(std::ostringstream &out, GraphicsStateCache &cache,
                    const FloatFormatter &fmt, const std::vector<Point> &pts,
                    const CanvasStroke &stroke) {
  if (pts.size() < 2)
    return;
  cache.SetStroke(out, stroke, fmt);
  out << fmt.Format(pts[0].x) << ' ' << fmt.Format(pts[0].y) << " m\n";
  for (size_t i = 1; i < pts.size(); ++i) {
    out << fmt.Format(pts[i].x) << ' ' << fmt.Format(pts[i].y) << " l\n";
  }
  out << "S\n";
}

void AppendPolygon(std::ostringstream &out, GraphicsStateCache &cache,
                   const FloatFormatter &fmt, const std::vector<Point> &pts,
                   const CanvasStroke &stroke, const CanvasFill *fill) {
  if (pts.size() < 3)
    return;
  auto emitPath = [&]() {
    out << fmt.Format(pts[0].x) << ' ' << fmt.Format(pts[0].y) << " m\n";
    for (size_t i = 1; i < pts.size(); ++i)
      out << fmt.Format(pts[i].x) << ' ' << fmt.Format(pts[i].y) << " l\n";
    out << "h\n";
  };

  if (stroke.width > 0.0f) {
    cache.SetStroke(out, stroke, fmt);
    emitPath();
    out << "S\n";
  }

  if (fill) {
    cache.SetFill(out, *fill, fmt);
    emitPath();
    out << "f\n";
  }
}

void AppendRectangle(std::ostringstream &out, GraphicsStateCache &cache,
                     const FloatFormatter &fmt, const Point &origin, double w,
                     double h, const CanvasStroke &stroke,
                     const CanvasFill *fill) {
  auto emitRect = [&]() {
    out << fmt.Format(origin.x) << ' ' << fmt.Format(origin.y) << ' '
        << fmt.Format(w) << ' ' << fmt.Format(h) << " re\n";
  };

  if (stroke.width > 0.0f) {
    cache.SetStroke(out, stroke, fmt);
    emitRect();
    out << "S\n";
  }

  if (fill) {
    cache.SetFill(out, *fill, fmt);
    emitRect();
    out << "f\n";
  }
}

void AppendCircle(std::ostringstream &out, GraphicsStateCache &cache,
                  const FloatFormatter &fmt, const Point &center,
                  double radius, const CanvasStroke &stroke,
                  const CanvasFill *fill) {
  // Approximate circle with 4 cubic Beziers.
  const double c = radius * 0.552284749831; // 4*(sqrt(2)-1)/3
  Point p0{center.x + radius, center.y};
  Point p1{center.x + radius, center.y + c};
  Point p2{center.x + c, center.y + radius};
  Point p3{center.x, center.y + radius};
  Point p4{center.x - c, center.y + radius};
  Point p5{center.x - radius, center.y + c};
  Point p6{center.x - radius, center.y};
  Point p7{center.x - radius, center.y - c};
  Point p8{center.x - c, center.y - radius};
  Point p9{center.x, center.y - radius};
  Point p10{center.x + c, center.y - radius};
  Point p11{center.x + radius, center.y - c};

  auto emitCircle = [&]() {
    out << fmt.Format(p0.x) << ' ' << fmt.Format(p0.y) << " m\n"
        << fmt.Format(p1.x) << ' ' << fmt.Format(p1.y) << ' '
        << fmt.Format(p2.x) << ' ' << fmt.Format(p2.y) << ' '
        << fmt.Format(p3.x) << ' ' << fmt.Format(p3.y) << " c\n"
        << fmt.Format(p4.x) << ' ' << fmt.Format(p4.y) << ' '
        << fmt.Format(p5.x) << ' ' << fmt.Format(p5.y) << ' '
        << fmt.Format(p6.x) << ' ' << fmt.Format(p6.y) << " c\n"
        << fmt.Format(p7.x) << ' ' << fmt.Format(p7.y) << ' '
        << fmt.Format(p8.x) << ' ' << fmt.Format(p8.y) << ' '
        << fmt.Format(p9.x) << ' ' << fmt.Format(p9.y) << " c\n"
        << fmt.Format(p10.x) << ' ' << fmt.Format(p10.y) << ' '
        << fmt.Format(p11.x) << ' ' << fmt.Format(p11.y) << ' '
        << fmt.Format(p0.x) << ' ' << fmt.Format(p0.y) << " c\n";
  };

  if (stroke.width > 0.0f) {
    cache.SetStroke(out, stroke, fmt);
    emitCircle();
    out << "S\n";
  }

  if (fill) {
    cache.SetFill(out, *fill, fmt);
    emitCircle();
    out << "f\n";
  }
}

void AppendText(std::ostringstream &out, const FloatFormatter &fmt,
                const Point &pos, const TextCommand &cmd,
                const CanvasTextStyle &style) {
  const auto glyphWidth = [](char ch) {
    static const std::unordered_map<char, int> widths = {
        {' ', 278}, {'!', 278}, {'"', 355}, {'#', 556}, {'$', 556},
        {'%', 889}, {'&', 667}, {'\'', 191}, {'(', 333}, {')', 333},
        {'*', 389}, {'+', 584}, {',', 278}, {'-', 333}, {'.', 278},
        {'/', 278}, {'0', 556}, {'1', 556}, {'2', 556}, {'3', 556},
        {'4', 556}, {'5', 556}, {'6', 556}, {'7', 556}, {'8', 556},
        {'9', 556}, {':', 278}, {';', 278}, {'<', 584}, {'=', 584},
        {'>', 584}, {'?', 556}, {'@', 1015}, {'A', 667}, {'B', 667},
        {'C', 722}, {'D', 722}, {'E', 667}, {'F', 611}, {'G', 778},
        {'H', 722}, {'I', 278}, {'J', 500}, {'K', 667}, {'L', 556},
        {'M', 833}, {'N', 722}, {'O', 778}, {'P', 667}, {'Q', 778},
        {'R', 722}, {'S', 667}, {'T', 611}, {'U', 722}, {'V', 667},
        {'W', 944}, {'X', 667}, {'Y', 667}, {'Z', 611}, {'[', 278},
        {'\\', 278}, {']', 278}, {'^', 469}, {'_', 556}, {'`', 333},
        {'a', 556}, {'b', 556}, {'c', 500}, {'d', 556}, {'e', 556},
        {'f', 278}, {'g', 556}, {'h', 556}, {'i', 222}, {'j', 222},
        {'k', 500}, {'l', 222}, {'m', 833}, {'n', 556}, {'o', 556},
        {'p', 556}, {'q', 556}, {'r', 333}, {'s', 500}, {'t', 278},
        {'u', 556}, {'v', 500}, {'w', 722}, {'x', 500}, {'y', 500},
        {'z', 500}, {'{', 334}, {'|', 260}, {'}', 334}, {'~', 584},
    };
    auto it = widths.find(ch);
    if (it != widths.end())
      return it->second;
    return 600; // Reasonable fallback for unknown glyphs
  };

  auto measureLineWidth = [&](std::string_view line) {
    int units = 0;
    for (char ch : line)
      units += glyphWidth(ch);
    return (units / 1000.0) * style.fontSize;
  };

  double maxLineWidth = 0.0;
  size_t lineStart = 0;
  for (size_t i = 0; i <= cmd.text.size(); ++i) {
    if (i == cmd.text.size() || cmd.text[i] == '\n') {
      maxLineWidth = std::max(
          maxLineWidth,
          measureLineWidth(std::string_view(cmd.text).substr(lineStart,
                                                              i - lineStart)));
      lineStart = i + 1;
    }
  }

  double horizontalOffset = 0.0;
  if (style.hAlign == CanvasTextStyle::HorizontalAlign::Center)
    horizontalOffset = -maxLineWidth / 2.0;
  else if (style.hAlign == CanvasTextStyle::HorizontalAlign::Right)
    horizontalOffset = -maxLineWidth;

  double verticalOffset = 0.0;
  if (style.vAlign == CanvasTextStyle::VerticalAlign::Top)
    verticalOffset = -style.fontSize * PDF_TEXT_ASCENT_FACTOR;

  const double lineAdvance = ComputeTextLineAdvance(style);
  out << "BT\n/F1 " << fmt.Format(style.fontSize) << " Tf\n";
  out << fmt.Format(style.color.r) << ' ' << fmt.Format(style.color.g) << ' '
      << fmt.Format(style.color.b) << " rg\n";
  out << fmt.Format(pos.x + horizontalOffset) << ' '
      << fmt.Format(pos.y + verticalOffset) << " Td\n";
  out << "(";
  for (char ch : cmd.text) {
    if (ch == '(' || ch == ')' || ch == '\\')
      out << '\\';
    if (ch == '\n') {
      out << ") Tj\n0 " << fmt.Format(lineAdvance) << " Td\n(";
      continue;
    }
    out << ch;
  }
  out << ") Tj\nET\n";
}

Point MapPointWithTransform(double x, double y, const Transform &current,
                            const Mapping &mapping) {
  auto applied = Apply(current, x, y);
  return MapWithMapping(applied.x, applied.y, mapping);
}

// Emits only the stroke portion of a drawing command. Keeping strokes and
// fills in separate functions allows the caller to control layering
// explicitly, which is required to match the on-screen 2D viewer where fills
// occlude internal wireframe edges within the same group.
void EmitCommandStroke(std::ostringstream &content, GraphicsStateCache &cache,
                       const FloatFormatter &formatter, const Mapping &mapping,
                       const Transform &current, const CanvasCommand &command) {
  std::visit(
      [&](auto &&c) {
        using T = std::decay_t<decltype(c)>;
        if constexpr (std::is_same_v<T, LineCommand>) {
          auto pa = MapPointWithTransform(c.x0, c.y0, current, mapping);
          auto pb = MapPointWithTransform(c.x1, c.y1, current, mapping);
          AppendLine(content, cache, formatter, pa, pb, c.stroke);
        } else if constexpr (std::is_same_v<T, PolylineCommand>) {
          std::vector<Point> pts;
          pts.reserve(c.points.size() / 2);
          for (size_t i = 0; i + 1 < c.points.size(); i += 2)
            pts.push_back(
                MapPointWithTransform(c.points[i], c.points[i + 1], current,
                                      mapping));
          AppendPolyline(content, cache, formatter, pts, c.stroke);
        } else if constexpr (std::is_same_v<T, PolygonCommand>) {
          std::vector<Point> pts;
          pts.reserve(c.points.size() / 2);
          for (size_t i = 0; i + 1 < c.points.size(); i += 2)
            pts.push_back(
                MapPointWithTransform(c.points[i], c.points[i + 1], current,
                                      mapping));
          AppendPolygon(content, cache, formatter, pts, c.stroke, nullptr);
        } else if constexpr (std::is_same_v<T, RectangleCommand>) {
          auto origin = MapPointWithTransform(c.x, c.y, current, mapping);
          double w = c.w * current.scale * mapping.scale;
          double h = c.h * current.scale * mapping.scale;
          AppendRectangle(content, cache, formatter, origin, w, h, c.stroke,
                          nullptr);
        } else if constexpr (std::is_same_v<T, CircleCommand>) {
          auto center = MapPointWithTransform(c.cx, c.cy, current, mapping);
          double radius = c.radius * current.scale * mapping.scale;
          AppendCircle(content, cache, formatter, center, radius, c.stroke,
                       nullptr);
        }
      },
      command);
}

// Emits only the fill portion of a drawing command. Stroke width is forced to
// zero to ensure no outlines leak back in when rendering fills as a separate
// pass.
void EmitCommandFill(std::ostringstream &content, GraphicsStateCache &cache,
                     const FloatFormatter &formatter, const Mapping &mapping,
                     const Transform &current, const CanvasCommand &command) {
  std::visit(
      [&](auto &&c) {
        using T = std::decay_t<decltype(c)>;
        if constexpr (std::is_same_v<T, PolygonCommand>) {
          std::vector<Point> pts;
          pts.reserve(c.points.size() / 2);
          for (size_t i = 0; i + 1 < c.points.size(); i += 2)
            pts.push_back(
                MapPointWithTransform(c.points[i], c.points[i + 1], current,
                                      mapping));
          CanvasStroke disabledStroke = c.stroke;
          disabledStroke.width = 0.0f;
          AppendPolygon(content, cache, formatter, pts, disabledStroke,
                        &c.fill);
        } else if constexpr (std::is_same_v<T, RectangleCommand>) {
          auto origin = MapPointWithTransform(c.x, c.y, current, mapping);
          double w = c.w * current.scale * mapping.scale;
          double h = c.h * current.scale * mapping.scale;
          CanvasStroke disabledStroke = c.stroke;
          disabledStroke.width = 0.0f;
          AppendRectangle(content, cache, formatter, origin, w, h,
                          disabledStroke, &c.fill);
        } else if constexpr (std::is_same_v<T, CircleCommand>) {
          auto center = MapPointWithTransform(c.cx, c.cy, current, mapping);
          double radius = c.radius * current.scale * mapping.scale;
          CanvasStroke disabledStroke = c.stroke;
          disabledStroke.width = 0.0f;
          AppendCircle(content, cache, formatter, center, radius,
                       disabledStroke, &c.fill);
        }
      },
      command);
}

std::string RenderCommandsToStream(
    const std::vector<CanvasCommand> &commands,
    const std::vector<CommandMetadata> &metadata,
    const std::vector<std::string> &sources, const Mapping &mapping,
    const FloatFormatter &formatter) {
  Transform current{};
  std::vector<Transform> stack;
  std::ostringstream content;
  GraphicsStateCache stateCache;

  std::vector<size_t> group;
  std::string currentSource;

  auto flushGroup = [&]() {
    if (group.empty())
      return;

    // Use dedicated buffers for strokes and fills so layering is explicit and
    // future exporters can reorder or post-process the layers independently.
    std::ostringstream strokeLayer;
    std::ostringstream fillLayer;

    // Render all strokes first. They will be visually pushed underneath by the
    // subsequent fill layer, mirroring how the real-time viewer relies on
    // depth testing to hide internal wireframe segments.
    for (size_t idx : group) {
      if (!metadata[idx].hasStroke)
        continue;
      EmitCommandStroke(strokeLayer, stateCache, formatter, mapping, current,
                        commands[idx]);
    }

    // Render fills afterwards so they sit on top of any wireframe lines from
    // the same piece, matching the 2D viewer's occlusion behavior.
    for (size_t idx : group) {
      if (!metadata[idx].hasFill)
        continue;
      EmitCommandFill(fillLayer, stateCache, formatter, mapping, current,
                      commands[idx]);
    }

    content << strokeLayer.str() << fillLayer.str();

    group.clear();
  };

  auto handleBarrier = [&](const auto &cmd) {
    using T = std::decay_t<decltype(cmd)>;
    if constexpr (std::is_same_v<T, SaveCommand>) {
      stack.push_back(current);
    } else if constexpr (std::is_same_v<T, RestoreCommand>) {
      if (!stack.empty()) {
        current = stack.back();
        stack.pop_back();
      }
    } else if constexpr (std::is_same_v<T, TransformCommand>) {
      current.scale = cmd.transform.scale;
      current.offsetX = cmd.transform.offsetX;
      current.offsetY = cmd.transform.offsetY;
    } else if constexpr (std::is_same_v<T, TextCommand>) {
      auto pos = MapPointWithTransform(cmd.x, cmd.y, current, mapping);
      AppendText(content, formatter, pos, cmd, cmd.style);
    } else {
      // Symbol control commands are handled at a higher level but must preserve
      // ordering relative to drawing commands.
    }
  };

  for (size_t i = 0; i < commands.size(); ++i) {
    const auto &cmd = commands[i];

    bool isBarrier = std::visit(
        [&](auto &&c) {
          using T = std::decay_t<decltype(c)>;
          return std::is_same_v<T, SaveCommand> || std::is_same_v<T, RestoreCommand> ||
                 std::is_same_v<T, TransformCommand> ||
                 std::is_same_v<T, BeginSymbolCommand> ||
                 std::is_same_v<T, EndSymbolCommand> ||
                 std::is_same_v<T, PlaceSymbolCommand> ||
                 std::is_same_v<T, TextCommand>;
        },
        cmd);

    if (isBarrier) {
      flushGroup();
      std::visit(handleBarrier, cmd);
      continue;
    }

    if (group.empty())
      currentSource = sources[i];

    if (sources[i] != currentSource) {
      flushGroup();
      currentSource = sources[i];
    }

    group.push_back(i);
  }

  flushGroup();

  return content.str();
}

std::string MakePdfName(const std::string &key) {
  std::string name = "X";
  for (char ch : key) {
    if (std::isalnum(static_cast<unsigned char>(ch)))
      name.push_back(ch);
    else
      name.push_back('_');
  }
  if (name.size() == 1)
    name += "Obj";
  return name;
}

} // namespace

PlanExportResult ExportPlanToPdf(const CommandBuffer &buffer,
                                 const Viewer2DViewState &viewState,
                                 const PlanPrintOptions &options,
                                 const std::filesystem::path &outputPath) {
  PlanExportResult result{};

  // Nothing to write if the render pass did not produce commands.
  if (buffer.commands.empty()) {
    result.message = "Nothing to export";
    return result;
  }

  // Fail fast when the output location is not usable to avoid performing any
  // rendering work that cannot be saved.
  if (outputPath.empty() || outputPath.filename().empty()) {
    result.message = "No output file was provided for the PDF plan.";
    return result;
  }

  const auto parent = outputPath.parent_path();
  std::error_code pathEc;
  if (!parent.empty() && !std::filesystem::exists(parent, pathEc)) {
    result.message = pathEc ?
                      "Unable to verify the selected folder for the PDF plan." :
                      "The selected folder does not exist.";
    return result;
  }

  // Validate viewport dimensions before calculating scales to avoid divide by
  // zero and produce a clear explanation for the caller.
  if (viewState.viewportWidth <= 0 || viewState.viewportHeight <= 0) {
    result.message = "The 2D viewport is not ready for export.";
    return result;
  }

  if (!std::isfinite(viewState.zoom) || viewState.zoom <= 0.0f) {
    result.message = "Invalid zoom value provided for export.";
    return result;
  }

  (void)viewState.view; // Orientation reserved for future layout tweaks.

  double pageW = options.landscape ? options.pageHeightPt : options.pageWidthPt;
  double pageH = options.landscape ? options.pageWidthPt : options.pageHeightPt;
  double margin = options.marginPt;
  double drawW = pageW - margin * 2.0;
  double drawH = pageH - margin * 2.0;
  // Ensure the paper configuration leaves a drawable area.
  if (drawW <= 0.0 || drawH <= 0.0) {
    result.message = "The selected paper size and margins leave no space for drawing.";
    return result;
  }

  double ppm = PIXELS_PER_METER * static_cast<double>(viewState.zoom);
  double halfW = static_cast<double>(viewState.viewportWidth) / ppm * 0.5;
  double halfH = static_cast<double>(viewState.viewportHeight) / ppm * 0.5;
  double offX = static_cast<double>(viewState.offsetPixelsX) / PIXELS_PER_METER;
  double offY = static_cast<double>(viewState.offsetPixelsY) / PIXELS_PER_METER;
  double minX = -halfW - offX;
  double maxX = halfW - offX;
  double minY = -halfH - offY;
  double maxY = halfH - offY;
  double width = maxX - minX;
  double height = maxY - minY;
  if (width <= 0.0 || height <= 0.0) {
    result.message = "Viewport dimensions are invalid for export.";
    return result;
  }

  double scale = std::min(drawW / width, drawH / height);
  double offsetX = margin + (drawW - width * scale) * 0.5;
  double offsetY = margin + (drawH - height * scale) * 0.5;

  FloatFormatter formatter(options.floatPrecision);

  struct SymbolPlacement {
    std::string key;
    CanvasTransform transform{};
  };

  struct CommandGroup {
    std::vector<CanvasCommand> commands;
    std::vector<CommandMetadata> metadata;
    std::vector<std::string> sources;
  };

  CommandGroup mainCommands;
  std::unordered_map<std::string, CommandGroup> symbolDefinitions;
  std::vector<SymbolPlacement> placements;
  std::string capturingKey;
  std::vector<CanvasCommand> captureBuffer;
  std::vector<CommandMetadata> captureMetadata;
  std::vector<std::string> captureSources;

  for (size_t i = 0; i < buffer.commands.size(); ++i) {
    const auto &cmd = buffer.commands[i];
    const auto &meta = buffer.metadata[i];
    const auto &source = buffer.sources[i];

    if (const auto *begin = std::get_if<BeginSymbolCommand>(&cmd)) {
      capturingKey = begin->key;
      captureBuffer.clear();
      captureMetadata.clear();
      captureSources.clear();
      continue;
    }
    if (const auto *end = std::get_if<EndSymbolCommand>(&cmd)) {
      if (!capturingKey.empty() && capturingKey == end->key &&
          !symbolDefinitions.count(capturingKey)) {
        symbolDefinitions.emplace(capturingKey,
                                  CommandGroup{captureBuffer, captureMetadata,
                                               captureSources});
      }
      capturingKey.clear();
      captureBuffer.clear();
      captureMetadata.clear();
      captureSources.clear();
      continue;
    }
    if (const auto *place = std::get_if<PlaceSymbolCommand>(&cmd)) {
      placements.push_back({place->key, place->transform});
      continue;
    }

    if (!capturingKey.empty()) {
      captureBuffer.push_back(cmd);
      captureMetadata.push_back(meta);
      captureSources.push_back(source);
      continue;
    }

    mainCommands.commands.push_back(cmd);
    mainCommands.metadata.push_back(meta);
    mainCommands.sources.push_back(source);
  }

  Mapping pageMapping{minX, minY, scale, offsetX, offsetY, pageH, false};
  std::string contentStr = RenderCommandsToStream(
      mainCommands.commands, mainCommands.metadata, mainCommands.sources,
      pageMapping, formatter);

  std::unordered_map<std::string, std::string> xObjectNames;
  std::unordered_map<std::string, size_t> xObjectIds;
  Mapping symbolMapping{};
  symbolMapping.flipY = false;

  for (const auto &entry : symbolDefinitions) {
    xObjectNames.emplace(entry.first, MakePdfName(entry.first));
  }

  std::ostringstream placementStream;
  for (const auto &placement : placements) {
    auto nameIt = xObjectNames.find(placement.key);
    if (nameIt == xObjectNames.end())
      continue;

    double a = placement.transform.scale * scale;
    double d = placement.transform.scale * scale;
    double e = scale * (placement.transform.offsetX - minX) + offsetX;
    double f = offsetY + scale * (placement.transform.offsetY - minY);

    placementStream << "q\n" << formatter.Format(a) << " 0 0 "
                    << formatter.Format(d) << ' ' << formatter.Format(e) << ' '
                    << formatter.Format(f) << " cm\n/" << nameIt->second
                    << " Do\nQ\n";
  }

  contentStr += placementStream.str();
  std::string compressedContent;
  bool useCompression = false;
  if (options.compressStreams) {
    std::string error;
    if (PdfDeflater::Compress(contentStr, compressedContent, error)) {
      useCompression = true;
    }
  }
  std::vector<PdfObject> objects;
  objects.push_back({"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>"});

  for (const auto &entry : symbolDefinitions) {
    std::string symbolContent =
        RenderCommandsToStream(entry.second.commands, entry.second.metadata,
                               entry.second.sources, symbolMapping, formatter);
    std::string compressedSymbol;
    bool compressed = false;
    if (options.compressStreams) {
      std::string error;
      compressed = PdfDeflater::Compress(symbolContent, compressedSymbol, error);
    }
    const std::string &symbolStream = compressed ? compressedSymbol : symbolContent;
    std::ostringstream xobj;
    xobj << "<< /Type /XObject /Subtype /Form /BBox [0 0 "
         << formatter.Format(width) << ' ' << formatter.Format(height)
         << "] /Resources << /Font << /F1 1 0 R >> >> /Length "
         << symbolStream.size();
    if (compressed)
      xobj << " /Filter /FlateDecode";
    xobj << " >>\nstream\n" << symbolStream << "endstream";
    objects.push_back({xobj.str()});
    xObjectIds[entry.first] = objects.size();
  }

  std::ostringstream contentObj;
  const std::string &streamData = useCompression ? compressedContent : contentStr;
  contentObj << "<< /Length " << streamData.size();
  if (useCompression)
    contentObj << " /Filter /FlateDecode";
  contentObj << " >>\nstream\n" << streamData << "endstream";
  objects.push_back({contentObj.str()});

  std::ostringstream resources;
  resources << "<< /Font << /F1 1 0 R >>";
  if (!xObjectIds.empty()) {
    resources << " /XObject << ";
    for (const auto &entry : xObjectIds) {
      auto nameIt = xObjectNames.find(entry.first);
      if (nameIt == xObjectNames.end())
        continue;
      resources << '/' << nameIt->second << ' ' << entry.second << " 0 R ";
    }
    resources << ">>";
  }
  resources << " >>";

  std::ostringstream pageObj;
  size_t contentIndex = objects.size();
  size_t pageIndex = contentIndex + 1;
  size_t pagesIndex = pageIndex + 1;
  size_t catalogIndex = pagesIndex + 1;

  pageObj << "<< /Type /Page /Parent " << pagesIndex << " 0 R /MediaBox [0 0 "
          << formatter.Format(pageW) << ' ' << formatter.Format(pageH)
          << "] /Contents " << contentIndex << " 0 R /Resources "
          << resources.str() << " >>";
  objects.push_back({pageObj.str()});
  objects.push_back({"<< /Type /Pages /Kids [" + std::to_string(pageIndex) +
                    " 0 R] /Count 1 >>"});
  objects.push_back({"<< /Type /Catalog /Pages " + std::to_string(pagesIndex) +
                    " 0 R >>"});

  try {
    std::ofstream file(outputPath, std::ios::binary);
    if (!file.is_open()) {
      result.message = "Unable to open the destination file for writing.";
      return result;
    }

    file << "%PDF-1.4\n";
    std::vector<long> offsets;
    offsets.reserve(objects.size());
    for (size_t i = 0; i < objects.size(); ++i) {
      offsets.push_back(static_cast<long>(file.tellp()));
      file << (i + 1) << " 0 obj\n" << objects[i].body << "\nendobj\n";
    }

    long xrefPos = static_cast<long>(file.tellp());
    file << "xref\n0 " << (objects.size() + 1)
         << "\n0000000000 65535 f \n";
    for (long off : offsets) {
      file << std::setw(10) << std::setfill('0') << off << " 00000 n \n";
    }
    file << "trailer\n<< /Size " << (objects.size() + 1)
         << " /Root " << catalogIndex
         << " 0 R >>\nstartxref\n" << xrefPos << "\n%%EOF";
    result.success = true;
  } catch (const std::exception &ex) {
    result.message = std::string("Failed to generate PDF content: ") + ex.what();
    return result;
  } catch (...) {
    result.message = "An unknown error occurred while generating the PDF plan.";
    return result;
  }

  return result;
}

