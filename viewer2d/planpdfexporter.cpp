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
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <system_error>
#include <type_traits>
#include <vector>

#include <zlib.h>

namespace {

constexpr float PIXELS_PER_METER = 25.0f;

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

Point Apply(const Transform &t, double x, double y) {
  return {x * t.scale + t.offsetX, y * t.scale + t.offsetY};
}

Point MapToPage(double x, double y, double minX, double minY, double scale,
                double offsetX, double offsetY, double pageHeight) {
  double px = offsetX + (x - minX) * scale;
  double py = offsetY + (y - minY) * scale;
  // Mirror the Y axis so the PDF uses the same top-left origin as the 2D
  // viewer. The OpenGL canvas grows upwards, while the PDF coordinate space
  // grows upwards from the bottom-left corner.
  py = pageHeight - py;
  return {px, py};
}

class GraphicsStateCache {
public:
  void SetStroke(std::ostringstream &out, const CanvasStroke &stroke,
                 const FloatFormatter &fmt) {
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
    CanvasStroke outlineStroke = stroke;
    outlineStroke.color = {0.0f, 0.0f, 0.0f, 1.0f};
    outlineStroke.width = 2.0f;
    cache.SetStroke(out, outlineStroke, fmt);
    emitPath();
    out << "S\n";
  }

  if (fill) {
    cache.SetFill(out, *fill, fmt);
    if (stroke.width > 0.0f) {
      cache.SetStroke(out, stroke, fmt);
      emitPath();
      out << "B\n";
    } else {
      emitPath();
      out << "f\n";
    }
  } else if (stroke.width > 0.0f) {
    cache.SetStroke(out, stroke, fmt);
    emitPath();
    out << "S\n";
  }
}

void AppendRectangle(std::ostringstream &out, GraphicsStateCache &cache,
                     const FloatFormatter &fmt, const Point &origin, double w,
                     double h, const CanvasStroke &stroke,
                     const CanvasFill *fill) {
  if (fill)
    cache.SetFill(out, *fill, fmt);
  out << fmt.Format(origin.x) << ' ' << fmt.Format(origin.y) << ' '
      << fmt.Format(w) << ' ' << fmt.Format(h) << " re\n";
  if (fill && stroke.width > 0.0f) {
    cache.SetStroke(out, stroke, fmt);
    out << "B\n";
  } else if (fill) {
    out << "f\n";
  } else if (stroke.width > 0.0f) {
    cache.SetStroke(out, stroke, fmt);
    out << "S\n";
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

  if (fill)
    cache.SetFill(out, *fill, fmt);

  out << fmt.Format(p0.x) << ' ' << fmt.Format(p0.y) << " m\n"
      << fmt.Format(p1.x) << ' ' << fmt.Format(p1.y) << ' ' << fmt.Format(p2.x)
      << ' ' << fmt.Format(p2.y) << ' ' << fmt.Format(p3.x) << ' '
      << fmt.Format(p3.y) << " c\n"
      << fmt.Format(p4.x) << ' ' << fmt.Format(p4.y) << ' ' << fmt.Format(p5.x)
      << ' ' << fmt.Format(p5.y) << ' ' << fmt.Format(p6.x) << ' '
      << fmt.Format(p6.y) << " c\n"
      << fmt.Format(p7.x) << ' ' << fmt.Format(p7.y) << ' ' << fmt.Format(p8.x)
      << ' ' << fmt.Format(p8.y) << ' ' << fmt.Format(p9.x) << ' '
      << fmt.Format(p9.y) << " c\n"
      << fmt.Format(p10.x) << ' ' << fmt.Format(p10.y) << ' '
      << fmt.Format(p11.x) << ' ' << fmt.Format(p11.y) << ' '
      << fmt.Format(p0.x) << ' ' << fmt.Format(p0.y) << " c\n";

  if (fill && stroke.width > 0.0f) {
    cache.SetStroke(out, stroke, fmt);
    out << "B\n";
  } else if (fill) {
    out << "f\n";
  } else if (stroke.width > 0.0f) {
    cache.SetStroke(out, stroke, fmt);
    out << "S\n";
  }
}

void AppendText(std::ostringstream &out, const FloatFormatter &fmt,
                const Point &pos, const TextCommand &cmd,
                const CanvasTextStyle &style) {
  (void)cmd;
  out << "BT\n/F1 " << fmt.Format(style.fontSize) << " Tf\n";
  out << fmt.Format(style.color.r) << ' ' << fmt.Format(style.color.g) << ' '
      << fmt.Format(style.color.b) << " rg\n";
  out << fmt.Format(pos.x) << ' ' << fmt.Format(pos.y) << " Td\n";
  out << "(";
  for (char ch : cmd.text) {
    if (ch == '(' || ch == ')' || ch == '\\')
      out << '\\';
    if (ch == '\n') {
      out << ") Tj\n0 " << -style.fontSize << " Td\n(";
      continue;
    }
    out << ch;
  }
  out << ") Tj\nET\n";
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

  Transform current{};
  std::vector<Transform> stack;
  std::ostringstream content;
  FloatFormatter formatter(options.floatPrecision);
  GraphicsStateCache stateCache;

  for (const auto &cmd : buffer.commands) {
    std::visit(
        [&](auto &&c) {
          using T = std::decay_t<decltype(c)>;
          if constexpr (std::is_same_v<T, SaveCommand>) {
            stack.push_back(current);
          } else if constexpr (std::is_same_v<T, RestoreCommand>) {
            if (!stack.empty()) {
              current = stack.back();
              stack.pop_back();
            }
          } else if constexpr (std::is_same_v<T, TransformCommand>) {
            current.scale = c.transform.scale;
            current.offsetX = c.transform.offsetX;
            current.offsetY = c.transform.offsetY;
          } else if constexpr (std::is_same_v<T, LineCommand>) {
            auto a = Apply(current, c.x0, c.y0);
            auto b = Apply(current, c.x1, c.y1);
            auto pa = MapToPage(a.x, a.y, minX, minY, scale, offsetX, offsetY,
                                pageH);
            auto pb = MapToPage(b.x, b.y, minX, minY, scale, offsetX, offsetY,
                                pageH);
            AppendLine(content, stateCache, formatter, pa, pb, c.stroke);
          } else if constexpr (std::is_same_v<T, PolylineCommand>) {
            std::vector<Point> pts;
            pts.reserve(c.points.size() / 2);
            for (size_t i = 0; i + 1 < c.points.size(); i += 2) {
              auto p = Apply(current, c.points[i], c.points[i + 1]);
              pts.push_back(MapToPage(p.x, p.y, minX, minY, scale, offsetX,
                                      offsetY, pageH));
            }
            AppendPolyline(content, stateCache, formatter, pts, c.stroke);
          } else if constexpr (std::is_same_v<T, PolygonCommand>) {
            std::vector<Point> pts;
            pts.reserve(c.points.size() / 2);
            for (size_t i = 0; i + 1 < c.points.size(); i += 2) {
              auto p = Apply(current, c.points[i], c.points[i + 1]);
              pts.push_back(MapToPage(p.x, p.y, minX, minY, scale, offsetX,
                                      offsetY, pageH));
            }
            const CanvasFill *fill = c.hasFill ? &c.fill : nullptr;
            AppendPolygon(content, stateCache, formatter, pts, c.stroke, fill);
          } else if constexpr (std::is_same_v<T, RectangleCommand>) {
            auto o = Apply(current, c.x, c.y);
            auto mapped =
                MapToPage(o.x, o.y, minX, minY, scale, offsetX, offsetY, pageH);
            double w = c.w * current.scale * scale;
            double h = c.h * current.scale * scale;
            const CanvasFill *fill = c.hasFill ? &c.fill : nullptr;
            AppendRectangle(content, stateCache, formatter, mapped, w, h,
                            c.stroke, fill);
          } else if constexpr (std::is_same_v<T, CircleCommand>) {
            auto c0 = Apply(current, c.cx, c.cy);
            auto mapped = MapToPage(c0.x, c0.y, minX, minY, scale, offsetX,
                                    offsetY, pageH);
            double radius = c.radius * current.scale * scale;
            const CanvasFill *fill = c.hasFill ? &c.fill : nullptr;
            AppendCircle(content, stateCache, formatter, mapped, radius,
                         c.stroke, fill);
          } else if constexpr (std::is_same_v<T, TextCommand>) {
            auto p = Apply(current, c.x, c.y);
            auto mapped = MapToPage(p.x, p.y, minX, minY, scale, offsetX,
                                    offsetY, pageH);
            AppendText(content, formatter, mapped, c, c.style);
          }
        },
        cmd);
  }

  std::string contentStr = content.str();
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
  std::ostringstream contentObj;
  const std::string &streamData = useCompression ? compressedContent : contentStr;
  contentObj << "<< /Length " << streamData.size();
  if (useCompression)
    contentObj << " /Filter /FlateDecode";
  contentObj << " >>\nstream\n" << streamData << "endstream";
  objects.push_back({contentObj.str()});

  std::ostringstream pageObj;
  pageObj << "<< /Type /Page /Parent 4 0 R /MediaBox [0 0 "
          << formatter.Format(pageW)
          << ' ' << formatter.Format(pageH)
          << "] /Contents 2 0 R /Resources << /Font << /F1 1 0 R >> >> >>";
  objects.push_back({pageObj.str()});
  objects.push_back({"<< /Type /Pages /Kids [3 0 R] /Count 1 >>"});
  objects.push_back({"<< /Type /Catalog /Pages 4 0 R >>"});

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
         << " /Root 5 0 R >>\nstartxref\n" << xrefPos << "\n%%EOF";
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

