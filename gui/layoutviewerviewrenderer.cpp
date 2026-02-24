#include "layoutviewerviewrenderer.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <unordered_map>

#include <wx/dcgraph.h>
#include <wx/dcmemory.h>

#include "symbols/PerastageSvgSymbol.h"
#include "viewer2dcommandrenderer.h"

namespace {
constexpr double kMillimetersToMeters = 0.001;
struct SvgLookupKey {
  std::string modelKey;
  SymbolViewKind view = SymbolViewKind::Top;

  bool operator==(const SvgLookupKey &other) const {
    return modelKey == other.modelKey && view == other.view;
  }
};

struct SvgLookupHasher {
  size_t operator()(const SvgLookupKey &key) const {
    return std::hash<std::string>{}(key.modelKey) ^
           (static_cast<size_t>(key.view) << 1);
  }
};


const PerastageSvgSymbolData *FindSvgSymbolCached(
    std::unordered_map<SvgLookupKey, std::optional<PerastageSvgSymbolData>,
                       SvgLookupHasher> &svgCache,
    const std::string &modelKey, SymbolViewKind requestedView) {
  auto lookup = [&](SymbolViewKind view) -> const PerastageSvgSymbolData * {
    SvgLookupKey cacheKey{modelKey, view};
    auto svgIt = svgCache.find(cacheKey);
    if (svgIt == svgCache.end()) {
      std::optional<PerastageSvgSymbolData> loaded;
      PerastageSvgSymbolData data;
      if (LoadPerastageSvgSymbolFromGdtf(modelKey, view, data)) {
        loaded = std::move(data);
      }
      svgIt = svgCache.emplace(std::move(cacheKey), std::move(loaded)).first;
    }
    return svgIt->second.has_value() ? &svgIt->second.value() : nullptr;
  };

  if (const PerastageSvgSymbolData *svg = lookup(requestedView)) {
    return svg;
  }

  if (requestedView == SymbolViewKind::Top) {
    return lookup(SymbolViewKind::Bottom);
  }

  return nullptr;
}

Transform2D ComposeTransform(const Transform2D &a, const Transform2D &b) {
  Transform2D out;
  out.a = a.a * b.a + a.c * b.b;
  out.b = a.b * b.a + a.d * b.b;
  out.c = a.a * b.c + a.c * b.d;
  out.d = a.b * b.c + a.d * b.d;
  out.tx = a.a * b.tx + a.c * b.ty + a.tx;
  out.ty = a.b * b.tx + a.d * b.ty + a.ty;
  return out;
}

viewer2d::Viewer2DRenderPoint MapPoint(const viewer2d::Viewer2DRenderMapping &mapping,
                                       const Transform2D &transform,
                                       float x, float y) {
  const double tx = transform.a * x + transform.c * y + transform.tx;
  const double ty = transform.b * x + transform.d * y + transform.ty;
  const double mappedX = mapping.offsetX + (tx - mapping.minX) * mapping.scale;
  const double mappedY =
      mapping.offsetY + mapping.drawHeight - (ty - mapping.minY) * mapping.scale;
  return {mappedX, mappedY};
}

wxPoint ToWxPoint(const viewer2d::Viewer2DRenderPoint &point) {
  return wxPoint(static_cast<int>(std::lround(point.x)),
                 static_cast<int>(std::lround(point.y)));
}

void DrawPolyline(wxDC &dc, const std::vector<viewer2d::Viewer2DRenderPoint> &points) {
  if (points.size() < 2)
    return;
  std::vector<wxPoint> wxPoints;
  wxPoints.reserve(points.size());
  for (const auto &point : points)
    wxPoints.push_back(ToWxPoint(point));
  dc.DrawLines(static_cast<int>(wxPoints.size()), wxPoints.data());
}

void DrawSvgSymbol(wxGCDC &dc, const viewer2d::Viewer2DRenderMapping &mapping,
                   const Transform2D &transform,
                   const PerastageSvgSymbolData &svg,
                   const wxColour &fillColor,
                   const wxColour &strokeColor) {
  wxGraphicsContext *gc = dc.GetGraphicsContext();
  if (!gc)
    return;

  auto appendPath = [&](wxGraphicsPath &path,
                        const std::vector<PerastageSvgPoint> &poly) {
    if (poly.size() < 3)
      return;
    auto start = MapPoint(mapping, transform,
                          static_cast<float>((poly.front().x + svg.offsetXmm) * kMillimetersToMeters),
                          static_cast<float>((poly.front().y + svg.offsetYmm) * kMillimetersToMeters));
    path.MoveToPoint(start.x, start.y);
    for (size_t i = 1; i < poly.size(); ++i) {
      auto mapped = MapPoint(mapping, transform,
                             static_cast<float>((poly[i].x + svg.offsetXmm) * kMillimetersToMeters),
                             static_cast<float>((poly[i].y + svg.offsetYmm) * kMillimetersToMeters));
      path.AddLineToPoint(mapped.x, mapped.y);
    }
    path.CloseSubpath();
  };

  gc->SetPen(*wxTRANSPARENT_PEN);
  gc->SetBrush(wxBrush(fillColor));
  for (const auto &polygon : svg.fills) {
    if (polygon.points.size() < 3)
      continue;
    wxGraphicsPath path = gc->CreatePath();
    appendPath(path, polygon.points);
    for (const auto &hole : polygon.holes)
      appendPath(path, hole);
    gc->FillPath(path, wxODDEVEN_RULE);
  }

  dc.SetPen(wxPen(strokeColor, 1));
  for (const auto &line : svg.strokes) {
    if (line.points.size() < 2)
      continue;
    std::vector<viewer2d::Viewer2DRenderPoint> mapped;
    mapped.reserve(line.points.size());
    for (const auto &point : line.points) {
      mapped.push_back(MapPoint(mapping, transform,
                                static_cast<float>((point.x + svg.offsetXmm) * kMillimetersToMeters),
                                static_cast<float>((point.y + svg.offsetYmm) * kMillimetersToMeters)));
    }
    DrawPolyline(dc, mapped);
  }
}


struct SvgGeometryMetrics {
  bool valid = false;
  double minX = 0.0;
  double minY = 0.0;
  double width = 0.0;
  double height = 0.0;
};

SvgGeometryMetrics ComputeSvgGeometryMetrics(const PerastageSvgSymbolData &svg) {
  SvgGeometryMetrics metrics;
  bool hasPoint = false;
  double minX = 0.0;
  double minY = 0.0;
  double maxX = 0.0;
  double maxY = 0.0;

  auto includePoint = [&](const PerastageSvgPoint &pt) {
    const double x = (pt.x + svg.offsetXmm) * kMillimetersToMeters;
    const double y = (pt.y + svg.offsetYmm) * kMillimetersToMeters;
    if (!hasPoint) {
      minX = maxX = x;
      minY = maxY = y;
      hasPoint = true;
      return;
    }
    minX = std::min(minX, x);
    minY = std::min(minY, y);
    maxX = std::max(maxX, x);
    maxY = std::max(maxY, y);
  };

  for (const auto &polygon : svg.fills) {
    for (const auto &pt : polygon.points)
      includePoint(pt);
    for (const auto &hole : polygon.holes)
      for (const auto &pt : hole)
        includePoint(pt);
  }
  for (const auto &line : svg.strokes)
    for (const auto &pt : line.points)
      includePoint(pt);

  if (!hasPoint)
    return metrics;

  metrics.minX = minX;
  metrics.minY = minY;
  metrics.width = std::max(0.0, maxX - minX);
  metrics.height = std::max(0.0, maxY - minY);
  metrics.valid = metrics.width > 0.0 && metrics.height > 0.0;
  return metrics;
}

Transform2D BuildSvgToSymbolTransform(const SymbolDefinition &symbol,
                                      const PerastageSvgSymbolData &svg) {
  Transform2D out = Transform2D::Identity();

  const SvgGeometryMetrics source = ComputeSvgGeometryMetrics(svg);
  const double targetW =
      static_cast<double>(symbol.bounds.max.x - symbol.bounds.min.x);
  const double targetH =
      static_cast<double>(symbol.bounds.max.y - symbol.bounds.min.y);
  if (!source.valid || targetW <= 0.0 || targetH <= 0.0)
    return out;

  const double uniformScale = std::min(targetW / source.width, targetH / source.height);
  if (!(uniformScale > 0.0))
    return out;

  const double drawW = source.width * uniformScale;
  const double drawH = source.height * uniformScale;
  const double targetMinX =
      static_cast<double>(symbol.bounds.min.x) + (targetW - drawW) * 0.5;
  const double targetMinY =
      static_cast<double>(symbol.bounds.min.y) + (targetH - drawH) * 0.5;

  out.a = static_cast<float>(uniformScale);
  out.d = static_cast<float>(uniformScale);
  out.tx = static_cast<float>(targetMinX - source.minX * uniformScale);
  out.ty = static_cast<float>(targetMinY - source.minY * uniformScale);
  return out;
}

void ResolveSymbolSvgColors(const SymbolDefinition &symbol,
                            wxColour &fillColor,
                            wxColour &strokeColor) {
  fillColor = wxColour(224, 224, 224);
  strokeColor = *wxBLACK;

  for (const auto &cmd : symbol.localCommands.commands) {
    if (const auto *polygon = std::get_if<PolygonCommand>(&cmd)) {
      if (polygon->hasFill) {
        fillColor = wxColour(
            static_cast<unsigned char>(std::clamp(polygon->fill.color.r, 0.0f, 1.0f) * 255.0f),
            static_cast<unsigned char>(std::clamp(polygon->fill.color.g, 0.0f, 1.0f) * 255.0f),
            static_cast<unsigned char>(std::clamp(polygon->fill.color.b, 0.0f, 1.0f) * 255.0f));
      }
      strokeColor = wxColour(
          static_cast<unsigned char>(std::clamp(polygon->stroke.color.r, 0.0f, 1.0f) * 255.0f),
          static_cast<unsigned char>(std::clamp(polygon->stroke.color.g, 0.0f, 1.0f) * 255.0f),
          static_cast<unsigned char>(std::clamp(polygon->stroke.color.b, 0.0f, 1.0f) * 255.0f));
      return;
    }
    if (const auto *rectangle = std::get_if<RectangleCommand>(&cmd)) {
      if (rectangle->hasFill) {
        fillColor = wxColour(
            static_cast<unsigned char>(std::clamp(rectangle->fill.color.r, 0.0f, 1.0f) * 255.0f),
            static_cast<unsigned char>(std::clamp(rectangle->fill.color.g, 0.0f, 1.0f) * 255.0f),
            static_cast<unsigned char>(std::clamp(rectangle->fill.color.b, 0.0f, 1.0f) * 255.0f));
      }
      strokeColor = wxColour(
          static_cast<unsigned char>(std::clamp(rectangle->stroke.color.r, 0.0f, 1.0f) * 255.0f),
          static_cast<unsigned char>(std::clamp(rectangle->stroke.color.g, 0.0f, 1.0f) * 255.0f),
          static_cast<unsigned char>(std::clamp(rectangle->stroke.color.b, 0.0f, 1.0f) * 255.0f));
      return;
    }
  }
}

void RenderCommandBuffer(wxGCDC &dc, const CommandBuffer &buffer,
                         const viewer2d::Viewer2DRenderMapping &mapping,
                         const SymbolDefinitionSnapshot *symbols,
                         std::unordered_map<SvgLookupKey,
                                            std::optional<PerastageSvgSymbolData>,
                                            SvgLookupHasher> &svgCache,
                         const Transform2D &localTransform,
                         const CanvasTransform &canvasTransform) {
  if (buffer.commands.empty())
    return;

  CanvasTransform currentTransform = canvasTransform;
  std::vector<CanvasTransform> transformStack;

  auto mapTransformedPoint = [&](float x, float y) {
    const double lx = localTransform.a * x + localTransform.c * y + localTransform.tx;
    const double ly = localTransform.b * x + localTransform.d * y + localTransform.ty;
    const double tx = lx * currentTransform.scale + currentTransform.offsetX;
    const double ty = ly * currentTransform.scale + currentTransform.offsetY;
    const double mappedX = mapping.offsetX + (tx - mapping.minX) * mapping.scale;
    const double mappedY =
        mapping.offsetY + mapping.drawHeight - (ty - mapping.minY) * mapping.scale;
    return viewer2d::Viewer2DRenderPoint{mappedX, mappedY};
  };

  auto drawPolygon = [&](const std::vector<float> &points, const CanvasStroke &stroke,
                         const CanvasFill *fill) {
    if (points.size() < 6)
      return;
    std::vector<wxPoint> wxPoints;
    wxPoints.reserve(points.size() / 2);
    for (size_t i = 0; i + 1 < points.size(); i += 2) {
      const auto mapped = mapTransformedPoint(points[i], points[i + 1]);
      wxPoints.push_back(ToWxPoint(mapped));
    }
    wxPen pen(wxColour(static_cast<unsigned char>(std::clamp(stroke.color.r, 0.0f, 1.0f) * 255.0f),
                       static_cast<unsigned char>(std::clamp(stroke.color.g, 0.0f, 1.0f) * 255.0f),
                       static_cast<unsigned char>(std::clamp(stroke.color.b, 0.0f, 1.0f) * 255.0f)),
              std::max(1, static_cast<int>(std::lround(stroke.width * mapping.scale))));
    dc.SetPen(pen);
    if (fill) {
      dc.SetBrush(wxBrush(wxColour(
          static_cast<unsigned char>(std::clamp(fill->color.r, 0.0f, 1.0f) * 255.0f),
          static_cast<unsigned char>(std::clamp(fill->color.g, 0.0f, 1.0f) * 255.0f),
          static_cast<unsigned char>(std::clamp(fill->color.b, 0.0f, 1.0f) * 255.0f))));
    } else {
      dc.SetBrush(*wxTRANSPARENT_BRUSH);
    }
    dc.DrawPolygon(static_cast<int>(wxPoints.size()), wxPoints.data());
  };

  for (const auto &cmd : buffer.commands) {
    if (const auto *line = std::get_if<LineCommand>(&cmd)) {
      const auto p0 = mapTransformedPoint(line->x0, line->y0);
      const auto p1 = mapTransformedPoint(line->x1, line->y1);
      dc.SetPen(wxPen(wxColour(static_cast<unsigned char>(line->stroke.color.r * 255.0f),
                               static_cast<unsigned char>(line->stroke.color.g * 255.0f),
                               static_cast<unsigned char>(line->stroke.color.b * 255.0f)),
                      std::max(1, static_cast<int>(std::lround(line->stroke.width * mapping.scale)))));
      dc.DrawLine(ToWxPoint(p0), ToWxPoint(p1));
    } else if (const auto *polyline = std::get_if<PolylineCommand>(&cmd)) {
      if (polyline->points.size() < 4)
        continue;
      std::vector<viewer2d::Viewer2DRenderPoint> points;
      points.reserve(polyline->points.size() / 2);
      for (size_t i = 0; i + 1 < polyline->points.size(); i += 2)
        points.push_back(mapTransformedPoint(polyline->points[i], polyline->points[i + 1]));
      dc.SetPen(wxPen(wxColour(static_cast<unsigned char>(polyline->stroke.color.r * 255.0f),
                               static_cast<unsigned char>(polyline->stroke.color.g * 255.0f),
                               static_cast<unsigned char>(polyline->stroke.color.b * 255.0f)),
                      std::max(1, static_cast<int>(std::lround(polyline->stroke.width * mapping.scale)))));
      DrawPolyline(dc, points);
    } else if (const auto *poly = std::get_if<PolygonCommand>(&cmd)) {
      drawPolygon(poly->points, poly->stroke, poly->hasFill ? &poly->fill : nullptr);
    } else if (const auto *rect = std::get_if<RectangleCommand>(&cmd)) {
      const std::vector<float> pts = {rect->x, rect->y, rect->x + rect->w, rect->y,
                                      rect->x + rect->w, rect->y + rect->h, rect->x,
                                      rect->y + rect->h};
      drawPolygon(pts, rect->stroke, rect->hasFill ? &rect->fill : nullptr);
    } else if (const auto *instance = std::get_if<SymbolInstanceCommand>(&cmd)) {
      if (!symbols)
        continue;
      auto it = symbols->find(instance->symbolId);
      if (it == symbols->end())
        continue;
      const Transform2D combined = ComposeTransform(localTransform, instance->transform);
      bool renderedSvg = false;
      if (!it->second.key.modelKey.empty()) {
        if (const PerastageSvgSymbolData *svg = FindSvgSymbolCached(
                svgCache, it->second.key.modelKey, it->second.key.viewKind)) {
          wxColour fillColor;
          wxColour strokeColor;
          ResolveSymbolSvgColors(it->second, fillColor, strokeColor);
          const Transform2D svgToSymbol =
              BuildSvgToSymbolTransform(it->second, *svg);
          DrawSvgSymbol(dc, mapping, ComposeTransform(combined, svgToSymbol),
                        *svg, fillColor, strokeColor);
          renderedSvg = true;
        }
      }
      if (!renderedSvg) {
        RenderCommandBuffer(dc, it->second.localCommands, mapping, symbols,
                            svgCache, combined, currentTransform);
      }
    } else if (const auto *save = std::get_if<SaveCommand>(&cmd)) {
      (void)save;
      transformStack.push_back(currentTransform);
    } else if (const auto *restore = std::get_if<RestoreCommand>(&cmd)) {
      (void)restore;
      if (!transformStack.empty()) {
        currentTransform = transformStack.back();
        transformStack.pop_back();
      }
    } else if (const auto *tf = std::get_if<TransformCommand>(&cmd)) {
      currentTransform = tf->transform;
    }
  }
}
} // namespace

wxImage RenderLayoutViewCommandBufferToImage(
    const wxSize &size, const CommandBuffer &buffer,
    const Viewer2DViewState &viewState,
    const SymbolDefinitionSnapshot *symbols) {
  if (size.GetWidth() <= 0 || size.GetHeight() <= 0)
    return wxImage();

  viewer2d::Viewer2DRenderMapping mapping{};
  if (!viewer2d::BuildViewMapping(viewState, static_cast<double>(size.GetWidth()),
                                  static_cast<double>(size.GetHeight()), 0.0,
                                  mapping)) {
    return wxImage();
  }

  wxBitmap bitmap(size.GetWidth(), size.GetHeight(), 32);
  wxMemoryDC memoryDc;
  memoryDc.SelectObject(bitmap);
  memoryDc.SetBackground(wxBrush(wxColour(255, 255, 255)));
  memoryDc.Clear();

  wxGCDC dc(memoryDc);
  std::unordered_map<SvgLookupKey, std::optional<PerastageSvgSymbolData>, SvgLookupHasher>
      svgCache;
  RenderCommandBuffer(dc, buffer, mapping, symbols, svgCache,
                      Transform2D::Identity(), CanvasTransform{});

  memoryDc.SelectObject(wxNullBitmap);
  return bitmap.ConvertToImage();
}
