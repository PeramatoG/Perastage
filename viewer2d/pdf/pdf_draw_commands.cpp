#include "pdf_draw_commands.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace layout_pdf_internal {

double ComputeTextLineAdvance(double ascent, double descent) {
  return -(ascent + descent);
}

Point Apply(const Transform &t, double x, double y) {
  return {x * t.scale + t.offsetX, y * t.scale + t.offsetY};
}

Point MapWithMapping(double x, double y, const Mapping &mapping) {
  double px = mapping.offsetX + (x - mapping.minX) * mapping.scale;
  double py = mapping.offsetY + (y - mapping.minY) * mapping.scale;
  if (mapping.flipY)
    py = mapping.offsetY + mapping.drawHeight -
         (y - mapping.minY) * mapping.scale;
  return {px, py};
}

namespace {
bool SameColor(const CanvasColor &a, const CanvasColor &b) {
  return std::abs(a.r - b.r) < 1e-6 && std::abs(a.g - b.g) < 1e-6 &&
         std::abs(a.b - b.b) < 1e-6;
}
} // namespace

void GraphicsStateCache::SetStroke(std::ostringstream &out,
                                   const CanvasStroke &stroke,
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

void GraphicsStateCache::SetFill(std::ostringstream &out,
                                 const CanvasFill &fill,
                                 const FloatFormatter &fmt) {
  if (!hasFillColor_ || !SameColor(fill.color, fillColor_)) {
    out << fmt.Format(fill.color.r) << ' ' << fmt.Format(fill.color.g) << ' '
        << fmt.Format(fill.color.b) << " rg\n";
    fillColor_ = fill.color;
    hasFillColor_ = true;
  }
}

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
                const CanvasTextStyle &style, double scale,
                const PdfFontCatalog *fonts) {
  const std::string encodedText = EncodeWinAnsi(cmd.text);
  const PdfFontDefinition *font =
      fonts ? fonts->Resolve(style.fontFamily) : nullptr;
  double scaledFontSize = style.fontSize * scale;
  if (font && font->embedded && font->metrics.unitsPerEm > 0 &&
      style.ascent > 0.0f && style.descent > 0.0f) {
    const double targetHeight = (style.ascent + style.descent) * scale;
    const double fontHeightUnits =
        font->metrics.ascent + std::abs(font->metrics.descent);
    if (fontHeightUnits > 0.0) {
      const double fontHeight =
          fontHeightUnits * scaledFontSize / font->metrics.unitsPerEm;
      if (fontHeight > 0.0)
        scaledFontSize *= targetHeight / fontHeight;
    }
  }

  auto measureLineWidth = [&](std::string_view line) {
    if (!font || !font->embedded)
      return static_cast<double>(line.size()) * scaledFontSize * 0.6;
    double units = 0.0;
    for (unsigned char ch : line) {
      units += font->metrics.advanceWidths[ch];
    }
    return (units / font->metrics.unitsPerEm) * scaledFontSize;
  };
  const double fallbackAscent =
      font && font->embedded
          ? (font->metrics.ascent * scaledFontSize / font->metrics.unitsPerEm)
          : scaledFontSize * 0.8;
  const double fallbackDescent =
      font && font->embedded
          ? (std::abs(font->metrics.descent) * scaledFontSize /
             font->metrics.unitsPerEm)
          : scaledFontSize * 0.2;
  const double ascent =
      style.ascent > 0.0f ? style.ascent * scale : fallbackAscent;
  const double descent =
      style.descent > 0.0f ? style.descent * scale : fallbackDescent;
  const double measuredLineHeight =
      style.lineHeight > 0.0f
          ? style.lineHeight * scale
          : (ascent + descent +
             (font && font->embedded
                  ? (font->metrics.lineGap * scaledFontSize /
                     font->metrics.unitsPerEm)
                  : 0.0));
  const double extraSpacing =
      style.lineHeight > 0.0f ? style.extraLineSpacing * scale : 0.0;

  double maxLineWidth = 0.0;
  size_t lineStart = 0;
  for (size_t i = 0; i <= encodedText.size(); ++i) {
    if (i == encodedText.size() || encodedText[i] == '\n') {
      maxLineWidth = std::max(
          maxLineWidth,
          measureLineWidth(std::string_view(encodedText)
                               .substr(lineStart, i - lineStart)));
      lineStart = i + 1;
    }
  }

  double horizontalOffset = 0.0;
  if (style.hAlign == CanvasTextStyle::HorizontalAlign::Center)
    horizontalOffset = -maxLineWidth / 2.0;
  else if (style.hAlign == CanvasTextStyle::HorizontalAlign::Right)
    horizontalOffset = -maxLineWidth;

  double verticalOffset = 0.0;
  switch (style.vAlign) {
  case CanvasTextStyle::VerticalAlign::Top:
    verticalOffset = -ascent;
    break;
  case CanvasTextStyle::VerticalAlign::Middle:
    verticalOffset = -(ascent - descent) * 0.5;
    break;
  case CanvasTextStyle::VerticalAlign::Bottom:
    verticalOffset = descent;
    break;
  case CanvasTextStyle::VerticalAlign::Baseline:
    break;
  }

  // Always advance downward for successive lines to mirror the on-screen
  // rendering, even if upstream metrics change sign conventions.
  double lineAdvance = 0.0;
  if (style.lineHeight > 0.0f) {
    lineAdvance = -(measuredLineHeight + extraSpacing);
  } else {
    lineAdvance = ComputeTextLineAdvance(ascent, descent);
  }
  if (lineAdvance > 0.0)
    lineAdvance = -lineAdvance;
  auto emitText = [&](const CanvasColor &color, double dx, double dy) {
    const char *fontKey = font ? font->key.c_str() : "F1";
    out << "BT\n/" << fontKey << ' ' << fmt.Format(scaledFontSize) << " Tf\n";
    out << fmt.Format(color.r) << ' ' << fmt.Format(color.g) << ' '
        << fmt.Format(color.b) << " rg\n";
    out << fmt.Format(pos.x + horizontalOffset + dx) << ' '
        << fmt.Format(pos.y + verticalOffset + dy) << " Td\n";
    out << "(";
    for (char ch : encodedText) {
      if (ch == '(' || ch == ')' || ch == '\\')
        out << '\\';
      if (ch == '\n') {
        out << ") Tj\n0 " << fmt.Format(lineAdvance) << " Td\n(";
        continue;
      }
      out << ch;
    }
    out << ") Tj\nET\n";
  };

  const double outline = style.outlineWidth * scale;
  if (outline > 0.0f) {
    const std::array<std::array<double, 2>, 8> offsets = {
        std::array<double, 2>{-outline, 0.0},
        std::array<double, 2>{outline, 0.0},
        std::array<double, 2>{0.0, -outline},
        std::array<double, 2>{0.0, outline},
        std::array<double, 2>{-outline, -outline},
        std::array<double, 2>{outline, -outline},
        std::array<double, 2>{-outline, outline},
        std::array<double, 2>{outline, outline}};
    for (const auto &offset : offsets)
      emitText(style.outlineColor, offset[0], offset[1]);
  }

  emitText(style.color, 0.0, 0.0);
}

Point MapPointWithTransform(double x, double y, const Transform &current,
                            const Mapping &mapping) {
  auto applied = Apply(current, x, y);
  return MapWithMapping(applied.x, applied.y, mapping);
}

Transform2D TransformFromCanvas(const CanvasTransform &transform) {
  Transform2D out{};
  out.a = transform.scale;
  out.d = transform.scale;
  out.tx = transform.offsetX;
  out.ty = transform.offsetY;
  return out;
}

void AppendSymbolInstance(std::ostringstream &out, const FloatFormatter &fmt,
                          const Mapping &mapping,
                          const Transform2D &transform,
                          const std::string &name) {
  const double linearScale = mapping.scale;
  double translateX = mapping.scale * transform.tx +
                      mapping.offsetX - mapping.minX * mapping.scale;
  double translateY = mapping.scale * transform.ty +
                      mapping.offsetY - mapping.minY * mapping.scale;
  out << "q\n" << fmt.Format(transform.a * linearScale) << ' '
      << fmt.Format(transform.b * linearScale) << ' '
      << fmt.Format(transform.c * linearScale) << ' '
      << fmt.Format(transform.d * linearScale)
      << ' ' << fmt.Format(translateX) << ' ' << fmt.Format(translateY)
      << " cm\n/" << name << " Do\nQ\n";
}

int SymbolViewRank(SymbolViewKind kind) {
  switch (kind) {
  case SymbolViewKind::Top:
    return 0;
  case SymbolViewKind::Bottom:
    return 1;
  case SymbolViewKind::Front:
    return 2;
  case SymbolViewKind::Left:
    return 3;
  case SymbolViewKind::Right:
    return 4;
  case SymbolViewKind::Back:
  default:
    return 5;
  }
}

const SymbolDefinition *FindSymbolDefinition(
    const SymbolDefinitionSnapshot *symbols, const std::string &modelKey) {
  if (!symbols || modelKey.empty())
    return nullptr;
  const SymbolDefinition *best = nullptr;
  int bestRank = std::numeric_limits<int>::max();
  for (const auto &entry : *symbols) {
    if (entry.second.key.modelKey != modelKey)
      continue;
    int rank = SymbolViewRank(entry.second.key.viewKind);
    if (!best || rank < bestRank) {
      best = &entry.second;
      bestRank = rank;
    }
  }
  return best;
}

const SymbolDefinition *FindSymbolDefinitionPreferred(
    const SymbolDefinitionSnapshot *symbols, const std::string &modelKey,
    SymbolViewKind preferred) {
  if (!symbols || modelKey.empty())
    return nullptr;
  for (const auto &entry : *symbols) {
    if (entry.second.key.modelKey == modelKey &&
        entry.second.key.viewKind == preferred) {
      return &entry.second;
    }
  }
  return FindSymbolDefinition(symbols, modelKey);
}

const SymbolDefinition *FindSymbolDefinitionExact(
    const SymbolDefinitionSnapshot *symbols, const std::string &modelKey,
    SymbolViewKind view) {
  if (!symbols || modelKey.empty())
    return nullptr;
  for (const auto &entry : *symbols) {
    if (entry.second.key.modelKey == modelKey &&
        entry.second.key.viewKind == view) {
      return &entry.second;
    }
  }
  return nullptr;
}

SymbolBounds ComputeSymbolBounds(const std::vector<CanvasCommand> &commands) {
  SymbolBounds bounds{};
  bool hasPoint = false;

  auto addPoint = [&](float x, float y) {
    if (!hasPoint) {
      bounds.min = {x, y};
      bounds.max = {x, y};
      hasPoint = true;
      return;
    }
    bounds.min.x = std::min(bounds.min.x, x);
    bounds.min.y = std::min(bounds.min.y, y);
    bounds.max.x = std::max(bounds.max.x, x);
    bounds.max.y = std::max(bounds.max.y, y);
  };

  auto addPointWithPadding = [&](float x, float y, float padding) {
    if (padding <= 0.0f) {
      addPoint(x, y);
      return;
    }
    addPoint(x - padding, y - padding);
    addPoint(x + padding, y + padding);
  };

  auto addPoints = [&](const std::vector<float> &points, float padding) {
    for (size_t i = 0; i + 1 < points.size(); i += 2)
      addPointWithPadding(points[i], points[i + 1], padding);
  };

  for (const auto &cmd : commands) {
    if (const auto *line = std::get_if<LineCommand>(&cmd)) {
      float padding = line->stroke.width * 0.5f;
      addPointWithPadding(line->x0, line->y0, padding);
      addPointWithPadding(line->x1, line->y1, padding);
    } else if (const auto *polyline = std::get_if<PolylineCommand>(&cmd)) {
      float padding = polyline->stroke.width * 0.5f;
      addPoints(polyline->points, padding);
    } else if (const auto *poly = std::get_if<PolygonCommand>(&cmd)) {
      float padding = poly->stroke.width * 0.5f;
      addPoints(poly->points, padding);
    } else if (const auto *rect = std::get_if<RectangleCommand>(&cmd)) {
      float padding = rect->stroke.width * 0.5f;
      addPoint(rect->x - padding, rect->y - padding);
      addPoint(rect->x + rect->w + padding, rect->y - padding);
      addPoint(rect->x + rect->w + padding, rect->y + rect->h + padding);
      addPoint(rect->x - padding, rect->y + rect->h + padding);
    } else if (const auto *circle = std::get_if<CircleCommand>(&cmd)) {
      float padding = circle->stroke.width * 0.5f;
      float radius = circle->radius + padding;
      addPoint(circle->cx - radius, circle->cy - radius);
      addPoint(circle->cx + radius, circle->cy + radius);
    }
  }

  if (!hasPoint) {
    bounds.min = {};
    bounds.max = {};
  }
  return bounds;
}

// Emits only the stroke portion of a drawing command. Keeping strokes and
// fills in separate functions allows the caller to control layering
// explicitly, which is required to match the on-screen 2D viewer where fills
// occlude internal wireframe edges within the same group.
void EmitCommandStroke(std::ostringstream &content, GraphicsStateCache &cache,
                       const FloatFormatter &formatter, const Mapping &mapping,
                       const Transform &current, const CanvasCommand &command,
                       const RenderOptions &options) {
  const double strokeScale = mapping.scale * options.strokeScale;
  std::visit(
      [&](auto &&c) {
        using T = std::decay_t<decltype(c)>;
        if constexpr (std::is_same_v<T, LineCommand>) {
          auto pa = MapPointWithTransform(c.x0, c.y0, current, mapping);
          auto pb = MapPointWithTransform(c.x1, c.y1, current, mapping);
          CanvasStroke stroke = c.stroke;
          stroke.width *= strokeScale;
          AppendLine(content, cache, formatter, pa, pb, stroke);
        } else if constexpr (std::is_same_v<T, PolylineCommand>) {
          std::vector<Point> pts;
          pts.reserve(c.points.size() / 2);
          for (size_t i = 0; i + 1 < c.points.size(); i += 2)
            pts.push_back(
                MapPointWithTransform(c.points[i], c.points[i + 1], current,
                                      mapping));
          CanvasStroke stroke = c.stroke;
          stroke.width *= strokeScale;
          AppendPolyline(content, cache, formatter, pts, stroke);
        } else if constexpr (std::is_same_v<T, PolygonCommand>) {
          std::vector<Point> pts;
          pts.reserve(c.points.size() / 2);
          for (size_t i = 0; i + 1 < c.points.size(); i += 2)
            pts.push_back(
                MapPointWithTransform(c.points[i], c.points[i + 1], current,
                                      mapping));
          CanvasStroke stroke = c.stroke;
          stroke.width *= strokeScale;
          AppendPolygon(content, cache, formatter, pts, stroke, nullptr);
        } else if constexpr (std::is_same_v<T, RectangleCommand>) {
          auto origin = MapPointWithTransform(c.x, c.y, current, mapping);
          double w = c.w * current.scale * mapping.scale;
          double h = c.h * current.scale * mapping.scale;
          CanvasStroke stroke = c.stroke;
          stroke.width *= strokeScale;
          AppendRectangle(content, cache, formatter, origin, w, h, stroke,
                          nullptr);
        } else if constexpr (std::is_same_v<T, CircleCommand>) {
          auto center = MapPointWithTransform(c.cx, c.cy, current, mapping);
          double radius = c.radius * current.scale * mapping.scale;
          CanvasStroke stroke = c.stroke;
          stroke.width *= strokeScale;
          AppendCircle(content, cache, formatter, center, radius, stroke,
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


} // namespace layout_pdf_internal
