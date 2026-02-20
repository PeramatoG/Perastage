#include "symboltools/symbol_from_viewer2d.h"

#include "symbolcache.h"
#include "viewer2dpanel.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <unordered_map>

namespace symboltools {
namespace {

struct Affine2D {
  float scale = 1.0f;
  float offsetX = 0.0f;
  float offsetY = 0.0f;
};

symbols::Point2D Apply(const Affine2D &t, float x, float y) {
  return {x * t.scale + t.offsetX, y * t.scale + t.offsetY};
}

void AddBounds(symbols::Aabb2D &bounds, bool &hasBounds,
               const symbols::Point2D &p) {
  if (!hasBounds) {
    bounds.minX = bounds.maxX = p.x;
    bounds.minY = bounds.maxY = p.y;
    hasBounds = true;
    return;
  }
  bounds.minX = std::min(bounds.minX, p.x);
  bounds.minY = std::min(bounds.minY, p.y);
  bounds.maxX = std::max(bounds.maxX, p.x);
  bounds.maxY = std::max(bounds.maxY, p.y);
}

void AddPolyline(symbols::Symbol2D &symbol, const std::vector<float> &points,
                 const Affine2D &transform) {
  if (points.size() < 4)
    return;
  symbols::Polyline2D line;
  line.points.reserve(points.size() / 2);
  for (size_t i = 0; i + 1 < points.size(); i += 2)
    line.points.push_back(Apply(transform, points[i], points[i + 1]));
  symbol.strokes.push_back(std::move(line));
}

void AddFillPolygon(symbols::Symbol2D &symbol, const std::vector<float> &points,
                    const Affine2D &transform) {
  if (points.size() < 6)
    return;
  symbols::PolygonWithHoles2D polygon;
  polygon.outer.reserve(points.size() / 2);
  for (size_t i = 0; i + 1 < points.size(); i += 2)
    polygon.outer.push_back(Apply(transform, points[i], points[i + 1]));
  symbol.fill.push_back(std::move(polygon));
}

std::vector<float> BuildCirclePoints(float cx, float cy, float radius,
                                     int segments) {
  std::vector<float> points;
  points.reserve(static_cast<size_t>(segments * 2));
  constexpr float kTwoPi = 6.28318530717958647692f;
  for (int i = 0; i < segments; ++i) {
    const float a = static_cast<float>(i) / static_cast<float>(segments) *
                    kTwoPi;
    points.push_back(cx + std::cos(a) * radius);
    points.push_back(cy + std::sin(a) * radius);
  }
  return points;
}

SymbolViewKind ToViewKind(symbols::SymbolView view) {
  switch (view) {
  case symbols::SymbolView::Front:
    return SymbolViewKind::Front;
  case symbols::SymbolView::Top:
    return SymbolViewKind::Top;
  case symbols::SymbolView::Bottom:
    return SymbolViewKind::Bottom;
  case symbols::SymbolView::Left:
    return SymbolViewKind::Left;
  }
  return SymbolViewKind::Top;
}

Viewer2DView ToViewerView(symbols::SymbolView view) {
  switch (view) {
  case symbols::SymbolView::Front:
    return Viewer2DView::Front;
  case symbols::SymbolView::Top:
    return Viewer2DView::Top;
  case symbols::SymbolView::Bottom:
    return Viewer2DView::Bottom;
  case symbols::SymbolView::Left:
    return Viewer2DView::Side;
  }
  return Viewer2DView::Top;
}

symbols::Symbol2D ConvertDefinition(const SymbolDefinition &definition,
                                    symbols::SymbolView view) {
  symbols::Symbol2D symbol;
  symbol.view = view;
  symbol.stroke_width_px = 2.0f;

  std::vector<Affine2D> stack;
  Affine2D current;

  bool hasBounds = false;

  for (const auto &command : definition.localCommands.commands) {
    if (const auto *line = std::get_if<LineCommand>(&command)) {
      symbols::Polyline2D outLine;
      outLine.points.push_back(Apply(current, line->x0, line->y0));
      outLine.points.push_back(Apply(current, line->x1, line->y1));
      symbol.strokes.push_back(outLine);
      AddBounds(symbol.bounds, hasBounds, outLine.points[0]);
      AddBounds(symbol.bounds, hasBounds, outLine.points[1]);
    } else if (const auto *polyline = std::get_if<PolylineCommand>(&command)) {
      AddPolyline(symbol, polyline->points, current);
      const auto &line = symbol.strokes.back();
      for (const auto &p : line.points)
        AddBounds(symbol.bounds, hasBounds, p);
    } else if (const auto *polygon = std::get_if<PolygonCommand>(&command)) {
      if (polygon->hasFill)
        AddFillPolygon(symbol, polygon->points, current);
      AddPolyline(symbol, polygon->points, current);
      if (!symbol.strokes.empty())
        symbol.strokes.back().closed = true;
      if (polygon->hasFill) {
        const auto &poly = symbol.fill.back();
        for (const auto &p : poly.outer)
          AddBounds(symbol.bounds, hasBounds, p);
      }
    } else if (const auto *rect = std::get_if<RectangleCommand>(&command)) {
      std::vector<float> points = {
          rect->x,          rect->y,          rect->x + rect->w,
          rect->y,          rect->x + rect->w, rect->y + rect->h,
          rect->x,          rect->y + rect->h};
      if (rect->hasFill)
        AddFillPolygon(symbol, points, current);
      AddPolyline(symbol, points, current);
      if (!symbol.strokes.empty())
        symbol.strokes.back().closed = true;
      if (rect->hasFill) {
        const auto &poly = symbol.fill.back();
        for (const auto &p : poly.outer)
          AddBounds(symbol.bounds, hasBounds, p);
      }
    } else if (const auto *circle = std::get_if<CircleCommand>(&command)) {
      auto points = BuildCirclePoints(circle->cx, circle->cy, circle->radius, 40);
      if (circle->hasFill)
        AddFillPolygon(symbol, points, current);
      AddPolyline(symbol, points, current);
      if (!symbol.strokes.empty())
        symbol.strokes.back().closed = true;
      if (circle->hasFill) {
        const auto &poly = symbol.fill.back();
        for (const auto &p : poly.outer)
          AddBounds(symbol.bounds, hasBounds, p);
      }
    } else if (const auto *save = std::get_if<SaveCommand>(&command)) {
      (void)save;
      stack.push_back(current);
    } else if (const auto *restore = std::get_if<RestoreCommand>(&command)) {
      (void)restore;
      if (!stack.empty()) {
        current = stack.back();
        stack.pop_back();
      }
    } else if (const auto *tf = std::get_if<TransformCommand>(&command)) {
      current.scale = tf->transform.scale;
      current.offsetX = tf->transform.offsetX;
      current.offsetY = tf->transform.offsetY;
    }
  }

  if (!hasBounds)
    symbol.bounds = symbols::Aabb2D{};
  return symbol;
}

} // namespace

bool BuildSymbolsFromViewer2DPipeline(Viewer2DPanel &panel,
                                      const std::string &modelKey,
                                      symbols::SymbolCollection &outSymbols,
                                      std::vector<std::string> &outLogLines) {
  outSymbols.clear();
  outLogLines.clear();

  const auto previousMode = panel.GetRenderMode();
  const auto previousView = panel.GetView();
  const Viewer2DRenderMode captureMode =
      previousMode == Viewer2DRenderMode::Wireframe ? Viewer2DRenderMode::White
                                                    : previousMode;

  panel.SetRenderMode(captureMode);
  for (const auto view : symbols::AllSymbolViews()) {
    panel.SetView(ToViewerView(view));
    panel.CaptureFrameNow([](CommandBuffer, Viewer2DViewState) {}, true, false);
  }

  const auto snapshot = panel.GetBottomSymbolCacheSnapshot();
  panel.SetView(previousView);
  panel.SetRenderMode(previousMode);

  if (!snapshot) {
    outLogLines.push_back("No symbol snapshot available from Viewer2D pipeline.");
    return false;
  }

  bool anyFound = false;
  for (const auto view : symbols::AllSymbolViews()) {
    const auto expectedKind = ToViewKind(view);
    const SymbolDefinition *found = nullptr;
    for (const auto &entry : *snapshot) {
      const auto &def = entry.second;
      if (def.key.modelKey == modelKey && def.key.viewKind == expectedKind) {
        found = &def;
        break;
      }
    }

    if (!found && view == symbols::SymbolView::Left) {
      for (const auto &entry : *snapshot) {
        const auto &def = entry.second;
        if (def.key.modelKey == modelKey && def.key.viewKind == SymbolViewKind::Right) {
          found = &def;
          break;
        }
      }
    }

    if (!found) {
      outLogLines.push_back(std::string("View ") + symbols::ToString(view) +
                            ": no symbol definition in Viewer2D cache");
      symbols::Symbol2D empty;
      empty.view = view;
      outSymbols.push_back(std::move(empty));
      continue;
    }

    anyFound = true;
    auto symbol = ConvertDefinition(*found, view);
    size_t fillVertices = 0;
    for (const auto &poly : symbol.fill)
      fillVertices += poly.outer.size();
    size_t strokePoints = 0;
    for (const auto &stroke : symbol.strokes)
      strokePoints += stroke.points.size();

    std::ostringstream ss;
    ss << "View " << symbols::ToString(view) << ": fill=" << symbol.fill.size()
       << " polygons, fillVertices=" << fillVertices << ", strokes="
       << symbol.strokes.size() << ", strokePoints=" << strokePoints;
    outLogLines.push_back(ss.str());
    outSymbols.push_back(std::move(symbol));
  }

  return anyFound;
}

} // namespace symboltools
