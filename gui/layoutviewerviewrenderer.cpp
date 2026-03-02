#include "layoutviewerviewrenderer.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <optional>
#include <unordered_map>

#include <wx/dcgraph.h>
#include <wx/dcmemory.h>

#include "configmanager.h"
#include "guiconfigservices.h"
#include "legendutils.h"
#include "symbols/PerastageSvgSymbol.h"
#include "viewer2dcommandrenderer.h"

namespace {
namespace fs = std::filesystem;
constexpr bool kDisableFallbackSymbolRenderingForDebug = false;

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


struct SvgGeometryBounds {
  double minX = 0.0;
  double minY = 0.0;
  double maxX = 0.0;
  double maxY = 0.0;
  bool valid = false;
};

SvgGeometryBounds ComputeSvgGeometryBounds(const PerastageSvgSymbolData &svg) {
  SvgGeometryBounds bounds;
  auto includePoint = [&](const PerastageSvgPoint &point) {
    const double x = point.x + svg.offsetXmm;
    const double y = point.y + svg.offsetYmm;
    if (!bounds.valid) {
      bounds.minX = bounds.maxX = x;
      bounds.minY = bounds.maxY = y;
      bounds.valid = true;
      return;
    }
    bounds.minX = std::min(bounds.minX, x);
    bounds.minY = std::min(bounds.minY, y);
    bounds.maxX = std::max(bounds.maxX, x);
    bounds.maxY = std::max(bounds.maxY, y);
  };

  for (const auto &polygon : svg.fills) {
    for (const auto &point : polygon.points)
      includePoint(point);
    for (const auto &hole : polygon.holes) {
      for (const auto &point : hole)
        includePoint(point);
    }
  }

  for (const auto &line : svg.strokes) {
    for (const auto &point : line.points)
      includePoint(point);
  }

  return bounds;
}

Transform2D BuildSvgToSymbolTransform(const PerastageSvgSymbolData &svg,
                                      const SymbolBounds &symbolBounds) {
  SvgGeometryBounds svgBounds;
  if (svg.viewBoxWidth > 0.0 && svg.viewBoxHeight > 0.0) {
    svgBounds.minX = svg.offsetXmm;
    svgBounds.minY = svg.offsetYmm;
    svgBounds.maxX = svg.offsetXmm + svg.viewBoxWidth;
    svgBounds.maxY = svg.offsetYmm + svg.viewBoxHeight;
    svgBounds.valid = true;
  } else {
    svgBounds = ComputeSvgGeometryBounds(svg);
  }

  const double srcW = svgBounds.maxX - svgBounds.minX;
  const double srcH = svgBounds.maxY - svgBounds.minY;
  const double dstW = static_cast<double>(symbolBounds.max.x - symbolBounds.min.x);
  const double dstH = static_cast<double>(symbolBounds.max.y - symbolBounds.min.y);
  if (!svgBounds.valid || srcW <= 0.0 || srcH <= 0.0 || dstW <= 0.0 || dstH <= 0.0)
    return Transform2D::Identity();

  const double sx = dstW / srcW;
  const double sy = dstH / srcH;

  Transform2D transform = Transform2D::Identity();
  transform.a = static_cast<float>(sx);
  transform.d = static_cast<float>(sy);
  transform.tx = static_cast<float>(symbolBounds.min.x - svgBounds.minX * sx);
  transform.ty = static_cast<float>(symbolBounds.min.y - svgBounds.minY * sy);
  return transform;
}

std::string NormalizePathSeparators(const std::string &path) {
  std::string out = path;
  const char sep = static_cast<char>(fs::path::preferred_separator);
  std::replace(out.begin(), out.end(), '\\', sep);
  return out;
}

std::string NormalizeModelPath(const std::string &path) {
  if (path.empty())
    return {};
  fs::path normalized(path);
  normalized = normalized.lexically_normal();
  return NormalizePathSeparators(normalized.string());
}

std::string ResolveSvgLookupModelKey(
    const std::string &modelKey, const std::string &sourceKey,
    std::unordered_map<std::string, std::string> &resolvedModelKeyCache) {
  const std::string cacheLookupKey = sourceKey.empty() ? modelKey
                                                        : (sourceKey + "\n" + modelKey);
  auto it = resolvedModelKeyCache.find(cacheLookupKey);
  if (it != resolvedModelKeyCache.end())
    return it->second;

  std::string resolved = modelKey;
  const std::string normalizedModel = NormalizeModelPath(modelKey);

  const auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  const auto &scene = cfg.GetScene();
  if (!sourceKey.empty()) {
    for (const auto &entry : scene.fixtures) {
      const Fixture &fixture = entry.second;
      if (fixture.typeName == sourceKey) {
        resolved = BuildFixtureSymbolKey(fixture, scene.basePath);
        break;
      }
    }
  }

  for (const auto &entry : scene.fixtures) {
    const Fixture &fixture = entry.second;
    const std::string fixtureSpec = NormalizeModelPath(fixture.gdtfSpec);
    if (!resolved.empty() && resolved != modelKey)
      break;
    if (!fixtureSpec.empty() && fixtureSpec == normalizedModel) {
      resolved = BuildFixtureSymbolKey(fixture, scene.basePath);
      break;
    }
    if (!fixture.typeName.empty() && fixture.typeName == modelKey) {
      resolved = BuildFixtureSymbolKey(fixture, scene.basePath);
      break;
    }
  }

  resolvedModelKeyCache.emplace(cacheLookupKey, resolved);
  return resolved;
}

const PerastageSvgSymbolData *FindSvgSymbolForView(
    const std::string &modelKey, const std::string &sourceKey,
    SymbolViewKind requestedView,
    std::unordered_map<SvgLookupKey, std::optional<PerastageSvgSymbolData>,
                       SvgLookupHasher> &svgCache,
    std::unordered_map<std::string, std::string> &resolvedModelKeyCache) {
  if (modelKey.empty())
    return nullptr;

  const std::string lookupModelKey =
      ResolveSvgLookupModelKey(modelKey, sourceKey, resolvedModelKeyCache);

  auto loadCached = [&](SymbolViewKind view) -> const PerastageSvgSymbolData * {
    const SvgLookupKey cacheKey{lookupModelKey, view};
    auto cacheIt = svgCache.find(cacheKey);
    if (cacheIt == svgCache.end()) {
      std::optional<PerastageSvgSymbolData> loaded;
      PerastageSvgSymbolData data;
      if (LoadPerastageSvgSymbolFromGdtf(lookupModelKey, view, data))
        loaded = std::move(data);
      cacheIt = svgCache.emplace(cacheKey, std::move(loaded)).first;
    }
    return cacheIt->second ? &cacheIt->second.value() : nullptr;
  };

  auto tryViews = [&](std::initializer_list<SymbolViewKind> views)
      -> const PerastageSvgSymbolData * {
    for (SymbolViewKind view : views) {
      if (const PerastageSvgSymbolData *data = loadCached(view))
        return data;
    }
    return nullptr;
  };

  switch (requestedView) {
  case SymbolViewKind::Top:
    return tryViews({SymbolViewKind::Top, SymbolViewKind::Bottom,
                     SymbolViewKind::Front, SymbolViewKind::Left,
                     SymbolViewKind::Right, SymbolViewKind::Back});
  case SymbolViewKind::Bottom:
    return tryViews({SymbolViewKind::Bottom, SymbolViewKind::Top,
                     SymbolViewKind::Front, SymbolViewKind::Left,
                     SymbolViewKind::Right, SymbolViewKind::Back});
  case SymbolViewKind::Front:
    return tryViews({SymbolViewKind::Front, SymbolViewKind::Top,
                     SymbolViewKind::Bottom, SymbolViewKind::Left,
                     SymbolViewKind::Right, SymbolViewKind::Back});
  case SymbolViewKind::Left:
    return tryViews({SymbolViewKind::Left, SymbolViewKind::Top,
                     SymbolViewKind::Bottom, SymbolViewKind::Front,
                     SymbolViewKind::Right, SymbolViewKind::Back});
  case SymbolViewKind::Right:
    return tryViews({SymbolViewKind::Right, SymbolViewKind::Top,
                     SymbolViewKind::Bottom, SymbolViewKind::Front,
                     SymbolViewKind::Left, SymbolViewKind::Back});
  case SymbolViewKind::Back:
  default:
    return tryViews({SymbolViewKind::Back, SymbolViewKind::Top,
                     SymbolViewKind::Bottom, SymbolViewKind::Front,
                     SymbolViewKind::Left, SymbolViewKind::Right});
  }
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

wxColour ToWxColor(const CanvasColor &color) {
  auto clamp = [](float value) -> unsigned char {
    return static_cast<unsigned char>(std::clamp(value, 0.0f, 1.0f) * 255.0f);
  };
  return wxColour(clamp(color.r), clamp(color.g), clamp(color.b),
                  clamp(color.a));
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
                   const PerastageSvgSymbolData &svg) {
  wxGraphicsContext *gc = dc.GetGraphicsContext();
  if (!gc)
    return;

  auto appendPath = [&](wxGraphicsPath &path,
                        const std::vector<PerastageSvgPoint> &poly) {
    if (poly.size() < 3)
      return;
    auto start = MapPoint(mapping, transform,
                          static_cast<float>(poly.front().x + svg.offsetXmm),
                          static_cast<float>(poly.front().y + svg.offsetYmm));
    path.MoveToPoint(start.x, start.y);
    for (size_t i = 1; i < poly.size(); ++i) {
      auto mapped = MapPoint(mapping, transform,
                             static_cast<float>(poly[i].x + svg.offsetXmm),
                             static_cast<float>(poly[i].y + svg.offsetYmm));
      path.AddLineToPoint(mapped.x, mapped.y);
    }
    path.CloseSubpath();
  };

  gc->SetPen(*wxTRANSPARENT_PEN);
  gc->SetBrush(wxBrush(wxColour(224, 224, 224)));
  for (const auto &polygon : svg.fills) {
    if (polygon.points.size() < 3)
      continue;
    wxGraphicsPath path = gc->CreatePath();
    appendPath(path, polygon.points);
    for (const auto &hole : polygon.holes)
      appendPath(path, hole);
    const auto fillRule =
        polygon.holes.empty() ? wxWINDING_RULE : wxODDEVEN_RULE;
    gc->FillPath(path, fillRule);
  }

  gc->SetPen(wxPen(*wxBLACK, 1));
  for (const auto &line : svg.strokes) {
    if (line.points.size() < 2)
      continue;
    wxGraphicsPath path = gc->CreatePath();
    auto mapped = MapPoint(mapping, transform,
                           static_cast<float>(line.points.front().x + svg.offsetXmm),
                           static_cast<float>(line.points.front().y + svg.offsetYmm));
    path.MoveToPoint(mapped.x, mapped.y);
    for (size_t i = 1; i < line.points.size(); ++i) {
      mapped = MapPoint(mapping, transform,
                        static_cast<float>(line.points[i].x + svg.offsetXmm),
                        static_cast<float>(line.points[i].y + svg.offsetYmm));
      path.AddLineToPoint(mapped.x, mapped.y);
    }
    gc->StrokePath(path);
  }
}


void RenderCommandBuffer(wxGCDC &dc, const CommandBuffer &buffer,
                         const viewer2d::Viewer2DRenderMapping &mapping,
                         const SymbolDefinitionSnapshot *symbols,
                         std::unordered_map<SvgLookupKey,
                                            std::optional<PerastageSvgSymbolData>,
                                            SvgLookupHasher> &svgCache,
                         std::unordered_map<std::string, std::string>
                             &resolvedModelKeyCache,
                         LayoutViewSvgOverlayDebugInfo *debugInfo,
                         const Transform2D &localTransform,
                         const CanvasTransform &canvasTransform,
                         bool renderSvgSymbolsOnly) {
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
    wxPen pen(ToWxColor(stroke.color),
              std::max(1, static_cast<int>(std::lround(stroke.width * mapping.scale))));
    dc.SetPen(pen);
    if (fill) {
      dc.SetBrush(wxBrush(ToWxColor(fill->color)));
    } else {
      dc.SetBrush(*wxTRANSPARENT_BRUSH);
    }
    dc.DrawPolygon(static_cast<int>(wxPoints.size()), wxPoints.data());
  };

  for (size_t cmdIndex = 0; cmdIndex < buffer.commands.size(); ++cmdIndex) {
    const auto &cmd = buffer.commands[cmdIndex];
    const std::string sourceKey =
        cmdIndex < buffer.sources.size() ? buffer.sources[cmdIndex] : std::string();
    if (const auto *line = std::get_if<LineCommand>(&cmd)) {
      if (renderSvgSymbolsOnly)
        continue;
      const auto p0 = mapTransformedPoint(line->x0, line->y0);
      const auto p1 = mapTransformedPoint(line->x1, line->y1);
      dc.SetPen(wxPen(ToWxColor(line->stroke.color),
                      std::max(1, static_cast<int>(std::lround(line->stroke.width * mapping.scale)))));
      dc.DrawLine(ToWxPoint(p0), ToWxPoint(p1));
    } else if (const auto *polyline = std::get_if<PolylineCommand>(&cmd)) {
      if (renderSvgSymbolsOnly)
        continue;
      if (polyline->points.size() < 4)
        continue;
      std::vector<viewer2d::Viewer2DRenderPoint> points;
      points.reserve(polyline->points.size() / 2);
      for (size_t i = 0; i + 1 < polyline->points.size(); i += 2)
        points.push_back(mapTransformedPoint(polyline->points[i], polyline->points[i + 1]));
      dc.SetPen(wxPen(ToWxColor(polyline->stroke.color),
                      std::max(1, static_cast<int>(std::lround(polyline->stroke.width * mapping.scale)))));
      DrawPolyline(dc, points);
    } else if (const auto *poly = std::get_if<PolygonCommand>(&cmd)) {
      if (renderSvgSymbolsOnly)
        continue;
      drawPolygon(poly->points, poly->stroke, poly->hasFill ? &poly->fill : nullptr);
    } else if (const auto *rect = std::get_if<RectangleCommand>(&cmd)) {
      if (renderSvgSymbolsOnly)
        continue;
      const std::vector<float> pts = {rect->x, rect->y, rect->x + rect->w, rect->y,
                                      rect->x + rect->w, rect->y + rect->h, rect->x,
                                      rect->y + rect->h};
      drawPolygon(pts, rect->stroke, rect->hasFill ? &rect->fill : nullptr);
    } else if (const auto *instance = std::get_if<SymbolInstanceCommand>(&cmd)) {
      if (debugInfo)
        debugInfo->symbolInstanceCommands += 1;
      if (!symbols)
        continue;
      auto it = symbols->find(instance->symbolId);
      if (it == symbols->end())
        continue;
      const Transform2D combined = ComposeTransform(localTransform, instance->transform);
      bool renderedSvg = false;

      if (!it->second.key.modelKey.empty()) {
        if (debugInfo)
          debugInfo->svgLookupAttempts += 1;
        if (!renderedSvg) {
          if (const PerastageSvgSymbolData *svg =
                  FindSvgSymbolForView(it->second.key.modelKey, sourceKey,
                                       it->second.key.viewKind, svgCache,
                                       resolvedModelKeyCache)) {
            const Transform2D svgTransform =
                ComposeTransform(combined,
                                 BuildSvgToSymbolTransform(*svg, it->second.bounds));
            DrawSvgSymbol(dc, mapping, svgTransform, *svg);
            renderedSvg = true;
            if (debugInfo)
              debugInfo->svgResolved += 1;
          }
        }
      }
      if (!renderedSvg && renderSvgSymbolsOnly)
        continue;

      if (!renderedSvg) {
        if (kDisableFallbackSymbolRenderingForDebug)
          continue;
        if (debugInfo)
          debugInfo->fallbackRenderCount += 1;
        RenderCommandBuffer(dc, it->second.localCommands, mapping, symbols,
                            svgCache, resolvedModelKeyCache, debugInfo,
                            combined, currentTransform,
                            renderSvgSymbolsOnly);
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
  std::unordered_map<std::string, std::string> resolvedModelKeyCache;
  RenderCommandBuffer(dc, buffer, mapping, symbols, svgCache,
                      resolvedModelKeyCache, nullptr, Transform2D::Identity(),
                      CanvasTransform{}, false);

  memoryDc.SelectObject(wxNullBitmap);
  return bitmap.ConvertToImage();
}

wxImage RenderLayoutViewSvgSymbolsOverlayToImage(
    const wxSize &size, const CommandBuffer &buffer,
    const Viewer2DViewState &viewState,
    const SymbolDefinitionSnapshot *symbols,
    LayoutViewSvgOverlayDebugInfo *debugInfo) {
  if (debugInfo)
    *debugInfo = LayoutViewSvgOverlayDebugInfo{};
  if (size.GetWidth() <= 0 || size.GetHeight() <= 0 || !symbols)
    return wxImage();

  viewer2d::Viewer2DRenderMapping mapping{};
  if (!viewer2d::BuildViewMapping(viewState, static_cast<double>(size.GetWidth()),
                                  static_cast<double>(size.GetHeight()), 0.0,
                                  mapping)) {
    return wxImage();
  }

  constexpr unsigned char kKeyR = 255;
  constexpr unsigned char kKeyG = 0;
  constexpr unsigned char kKeyB = 255;

  wxBitmap bitmap(size.GetWidth(), size.GetHeight(), 32);
  wxMemoryDC memoryDc;
  memoryDc.SelectObject(bitmap);
  memoryDc.SetBackground(wxBrush(wxColour(kKeyR, kKeyG, kKeyB)));
  memoryDc.Clear();

  wxGCDC dc(memoryDc);
  if (wxGraphicsContext *gc = dc.GetGraphicsContext())
    gc->SetAntialiasMode(wxANTIALIAS_NONE);
  std::unordered_map<SvgLookupKey, std::optional<PerastageSvgSymbolData>, SvgLookupHasher>
      svgCache;
  std::unordered_map<std::string, std::string> resolvedModelKeyCache;
  RenderCommandBuffer(dc, buffer, mapping, symbols, svgCache,
                      resolvedModelKeyCache, debugInfo, Transform2D::Identity(),
                      CanvasTransform{}, true);

  memoryDc.SelectObject(wxNullBitmap);
  wxImage image = bitmap.ConvertToImage();
  if (!image.IsOk())
    return wxImage();
  if (!image.HasAlpha())
    image.InitAlpha();

  unsigned char *rgb = image.GetData();
  unsigned char *alpha = image.GetAlpha();
  if (!rgb || !alpha)
    return wxImage();

  const size_t pixelCount = static_cast<size_t>(image.GetWidth()) *
                            static_cast<size_t>(image.GetHeight());
  for (size_t i = 0; i < pixelCount; ++i) {
    const size_t rgbIndex = i * 3;
    const bool isKey = rgb[rgbIndex] == kKeyR && rgb[rgbIndex + 1] == kKeyG &&
                       rgb[rgbIndex + 2] == kKeyB;
    alpha[i] = isKey ? 0 : 255;
  }

  return image;
}
