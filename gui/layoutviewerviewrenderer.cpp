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
    const std::string &modelKey,
    std::unordered_map<std::string, std::string> &resolvedModelKeyCache) {
  auto it = resolvedModelKeyCache.find(modelKey);
  if (it != resolvedModelKeyCache.end())
    return it->second;

  std::string resolved = modelKey;
  const std::string normalizedModel = NormalizeModelPath(modelKey);

  const auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  const auto &scene = cfg.GetScene();
  for (const auto &entry : scene.fixtures) {
    const Fixture &fixture = entry.second;
    const std::string fixtureSpec = NormalizeModelPath(fixture.gdtfSpec);
    if (!fixtureSpec.empty() && fixtureSpec == normalizedModel) {
      resolved = BuildFixtureSymbolKey(fixture, scene.basePath);
      break;
    }
    if (!fixture.typeName.empty() && fixture.typeName == modelKey) {
      resolved = BuildFixtureSymbolKey(fixture, scene.basePath);
      break;
    }
  }

  resolvedModelKeyCache.emplace(modelKey, resolved);
  return resolved;
}

const PerastageSvgSymbolData *FindSvgSymbolForView(
    const std::string &modelKey, SymbolViewKind requestedView,
    std::unordered_map<SvgLookupKey, std::optional<PerastageSvgSymbolData>,
                       SvgLookupHasher> &svgCache,
    std::unordered_map<std::string, std::string> &resolvedModelKeyCache) {
  if (modelKey.empty())
    return nullptr;

  const std::string lookupModelKey =
      ResolveSvgLookupModelKey(modelKey, resolvedModelKeyCache);

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

  if (const PerastageSvgSymbolData *exact = loadCached(requestedView))
    return exact;

  if (requestedView == SymbolViewKind::Bottom)
    return loadCached(SymbolViewKind::Top);
  if (requestedView == SymbolViewKind::Top)
    return loadCached(SymbolViewKind::Bottom);

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
    gc->FillPath(path, wxODDEVEN_RULE);
  }

  dc.SetPen(wxPen(*wxBLACK, 1));
  for (const auto &line : svg.strokes) {
    if (line.points.size() < 2)
      continue;
    std::vector<viewer2d::Viewer2DRenderPoint> mapped;
    mapped.reserve(line.points.size());
    for (const auto &point : line.points) {
      mapped.push_back(MapPoint(mapping, transform,
                                static_cast<float>(point.x + svg.offsetXmm),
                                static_cast<float>(point.y + svg.offsetYmm)));
    }
    DrawPolyline(dc, mapped);
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
      if (renderSvgSymbolsOnly)
        continue;
      const auto p0 = mapTransformedPoint(line->x0, line->y0);
      const auto p1 = mapTransformedPoint(line->x1, line->y1);
      dc.SetPen(wxPen(wxColour(static_cast<unsigned char>(line->stroke.color.r * 255.0f),
                               static_cast<unsigned char>(line->stroke.color.g * 255.0f),
                               static_cast<unsigned char>(line->stroke.color.b * 255.0f)),
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
      dc.SetPen(wxPen(wxColour(static_cast<unsigned char>(polyline->stroke.color.r * 255.0f),
                               static_cast<unsigned char>(polyline->stroke.color.g * 255.0f),
                               static_cast<unsigned char>(polyline->stroke.color.b * 255.0f)),
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
        if (const PerastageSvgSymbolData *svg =
                FindSvgSymbolForView(it->second.key.modelKey,
                                     it->second.key.viewKind, svgCache,
                                     resolvedModelKeyCache)) {
          DrawSvgSymbol(dc, mapping, combined, *svg);
          renderedSvg = true;
          if (debugInfo)
            debugInfo->svgResolved += 1;
        }
      }
      if (!renderedSvg && !renderSvgSymbolsOnly) {
        if (debugInfo)
          debugInfo->fallbackRenderCount += 1;
        RenderCommandBuffer(dc, it->second.localCommands, mapping, symbols,
                            svgCache, resolvedModelKeyCache, debugInfo, combined,
                            currentTransform,
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
