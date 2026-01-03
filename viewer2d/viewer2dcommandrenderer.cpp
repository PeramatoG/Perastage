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

#include "viewer2dcommandrenderer.h"

#include <algorithm>
#include <cmath>

#include "viewer2dpanel.h"

namespace viewer2d {
namespace {
struct LocalPoint {
  double x = 0.0;
  double y = 0.0;
};

LocalPoint ApplyTransformPoint(const Transform2D &t, float x, float y) {
  return {t.a * x + t.c * y + t.tx, t.b * x + t.d * y + t.ty};
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
} // namespace

bool ComputeViewBounds(const Viewer2DViewState &viewState,
                       Viewer2DViewBounds &bounds) {
  if (viewState.viewportWidth <= 0 || viewState.viewportHeight <= 0)
    return false;
  if (!std::isfinite(viewState.zoom) || viewState.zoom <= 0.0f)
    return false;

  double ppm = kViewer2DPixelsPerMeter * static_cast<double>(viewState.zoom);
  double halfW = static_cast<double>(viewState.viewportWidth) / ppm * 0.5;
  double halfH = static_cast<double>(viewState.viewportHeight) / ppm * 0.5;
  double offX = static_cast<double>(viewState.offsetPixelsX) /
                kViewer2DPixelsPerMeter;
  double offY = static_cast<double>(viewState.offsetPixelsY) /
                kViewer2DPixelsPerMeter;
  double minX = -halfW - offX;
  double maxX = halfW - offX;
  double minY = -halfH - offY;
  double maxY = halfH - offY;
  double width = maxX - minX;
  double height = maxY - minY;
  if (width <= 0.0 || height <= 0.0)
    return false;

  bounds.minX = minX;
  bounds.minY = minY;
  bounds.maxX = maxX;
  bounds.maxY = maxY;
  bounds.width = width;
  bounds.height = height;
  return true;
}

bool BuildViewMapping(const Viewer2DViewState &viewState, double targetWidth,
                      double targetHeight, double margin,
                      Viewer2DRenderMapping &mapping) {
  Viewer2DViewBounds bounds;
  if (!ComputeViewBounds(viewState, bounds))
    return false;

  if (targetWidth <= 0.0 || targetHeight <= 0.0)
    return false;

  double drawW = targetWidth - margin * 2.0;
  double drawH = targetHeight - margin * 2.0;
  if (drawW <= 0.0 || drawH <= 0.0)
    return false;

  double scale = std::min(drawW / bounds.width, drawH / bounds.height);
  double offsetX = margin + (drawW - bounds.width * scale) * 0.5;
  double offsetY = margin + (drawH - bounds.height * scale) * 0.5;

  mapping.minX = bounds.minX;
  mapping.minY = bounds.minY;
  mapping.scale = scale;
  mapping.offsetX = offsetX;
  mapping.offsetY = offsetY;
  mapping.drawHeight = bounds.height * scale;
  return true;
}

Viewer2DCommandRenderer::Viewer2DCommandRenderer(
    const Viewer2DRenderMapping &mapping, IViewer2DCommandBackend &backend,
    const SymbolDefinitionSnapshot *symbols)
    : mapping_(mapping), backend_(backend), symbols_(symbols) {}

void Viewer2DCommandRenderer::Render(const CommandBuffer &buffer,
                                     const Transform2D &localTransform) {
  RenderInternal(buffer, localTransform);
}

void Viewer2DCommandRenderer::RenderInternal(const CommandBuffer &buffer,
                                             const Transform2D &localTransform) {
  if (buffer.commands.empty())
    return;

  CanvasTransform currentTransform{};
  std::vector<CanvasTransform> stack;
  std::vector<size_t> group;
  std::string currentSource;

  auto strokeWidth = [&](float width) {
    return width * currentTransform.scale * mapping_.scale;
  };

  auto mapPoint = [&](float x, float y) {
    LocalPoint transformed = ApplyTransformPoint(localTransform, x, y);
    double tx = transformed.x * currentTransform.scale +
                currentTransform.offsetX;
    double ty = transformed.y * currentTransform.scale +
                currentTransform.offsetY;
    double mappedX = mapping_.offsetX + (tx - mapping_.minX) * mapping_.scale;
    double mappedY = mapping_.offsetY + mapping_.drawHeight -
                     (ty - mapping_.minY) * mapping_.scale;
    return Viewer2DRenderPoint{mappedX, mappedY};
  };

  auto drawSymbolInstance = [&](uint32_t symbolId,
                                const Transform2D &transform) -> void {
    if (!symbols_)
      return;
    auto it = symbols_->find(symbolId);
    if (it == symbols_->end())
      return;
    Transform2D combined = ComposeTransform(localTransform, transform);
    RenderInternal(it->second.localCommands, combined);
  };

  auto drawStrokeCommand = [&](const CanvasCommand &cmd) {
    if (const auto *line = std::get_if<LineCommand>(&cmd)) {
      Viewer2DRenderPoint p0 = mapPoint(line->x0, line->y0);
      Viewer2DRenderPoint p1 = mapPoint(line->x1, line->y1);
      backend_.DrawLine(p0, p1, line->stroke, strokeWidth(line->stroke.width));
    } else if (const auto *polyline = std::get_if<PolylineCommand>(&cmd)) {
      if (polyline->points.size() < 4)
        return;
      std::vector<Viewer2DRenderPoint> points;
      points.reserve(polyline->points.size() / 2);
      for (size_t i = 0; i + 1 < polyline->points.size(); i += 2) {
        points.push_back(mapPoint(polyline->points[i],
                                  polyline->points[i + 1]));
      }
      backend_.DrawPolyline(points, polyline->stroke,
                            strokeWidth(polyline->stroke.width));
    } else if (const auto *poly = std::get_if<PolygonCommand>(&cmd)) {
      if (poly->points.size() < 6)
        return;
      std::vector<Viewer2DRenderPoint> points;
      points.reserve(poly->points.size() / 2);
      for (size_t i = 0; i + 1 < poly->points.size(); i += 2) {
        points.push_back(mapPoint(poly->points[i], poly->points[i + 1]));
      }
      backend_.DrawPolygon(points, poly->stroke, nullptr,
                           strokeWidth(poly->stroke.width));
    } else if (const auto *rect = std::get_if<RectangleCommand>(&cmd)) {
      std::vector<float> pts = {rect->x, rect->y, rect->x + rect->w, rect->y,
                                rect->x + rect->w, rect->y + rect->h, rect->x,
                                rect->y + rect->h};
      std::vector<Viewer2DRenderPoint> points;
      points.reserve(pts.size() / 2);
      for (size_t i = 0; i + 1 < pts.size(); i += 2) {
        points.push_back(mapPoint(pts[i], pts[i + 1]));
      }
      backend_.DrawPolygon(points, rect->stroke, nullptr,
                           strokeWidth(rect->stroke.width));
    } else if (const auto *circle = std::get_if<CircleCommand>(&cmd)) {
      Viewer2DRenderPoint center = mapPoint(circle->cx, circle->cy);
      float sx = std::sqrt(localTransform.a * localTransform.a +
                           localTransform.b * localTransform.b);
      float sy = std::sqrt(localTransform.c * localTransform.c +
                           localTransform.d * localTransform.d);
      float scale = (sx + sy) * 0.5f;
      double radius = circle->radius * scale * currentTransform.scale *
                      mapping_.scale;
      backend_.DrawCircle(center, radius, circle->stroke, nullptr,
                          strokeWidth(circle->stroke.width));
    }
  };

  auto drawFillCommand = [&](const CanvasCommand &cmd) {
    if (const auto *poly = std::get_if<PolygonCommand>(&cmd)) {
      if (poly->points.size() < 6)
        return;
      std::vector<Viewer2DRenderPoint> points;
      points.reserve(poly->points.size() / 2);
      for (size_t i = 0; i + 1 < poly->points.size(); i += 2) {
        points.push_back(mapPoint(poly->points[i], poly->points[i + 1]));
      }
      CanvasStroke stroke = poly->stroke;
      stroke.width = 0.0f;
      const CanvasFill *fill = poly->hasFill ? &poly->fill : nullptr;
      backend_.DrawPolygon(points, stroke, fill, 0.0);
    } else if (const auto *rect = std::get_if<RectangleCommand>(&cmd)) {
      std::vector<float> pts = {rect->x, rect->y, rect->x + rect->w, rect->y,
                                rect->x + rect->w, rect->y + rect->h, rect->x,
                                rect->y + rect->h};
      std::vector<Viewer2DRenderPoint> points;
      points.reserve(pts.size() / 2);
      for (size_t i = 0; i + 1 < pts.size(); i += 2) {
        points.push_back(mapPoint(pts[i], pts[i + 1]));
      }
      CanvasStroke stroke = rect->stroke;
      stroke.width = 0.0f;
      const CanvasFill *fill = rect->hasFill ? &rect->fill : nullptr;
      backend_.DrawPolygon(points, stroke, fill, 0.0);
    } else if (const auto *circle = std::get_if<CircleCommand>(&cmd)) {
      Viewer2DRenderPoint center = mapPoint(circle->cx, circle->cy);
      float sx = std::sqrt(localTransform.a * localTransform.a +
                           localTransform.b * localTransform.b);
      float sy = std::sqrt(localTransform.c * localTransform.c +
                           localTransform.d * localTransform.d);
      float scale = (sx + sy) * 0.5f;
      double radius = circle->radius * scale * currentTransform.scale *
                      mapping_.scale;
      CanvasStroke stroke = circle->stroke;
      stroke.width = 0.0f;
      const CanvasFill *fill = circle->hasFill ? &circle->fill : nullptr;
      backend_.DrawCircle(center, radius, stroke, fill, 0.0);
    }
  };

  auto flushGroup = [&]() {
    if (group.empty())
      return;
    for (size_t idx : group) {
      if (idx >= buffer.metadata.size())
        continue;
      if (buffer.metadata[idx].hasStroke)
        drawStrokeCommand(buffer.commands[idx]);
    }
    for (size_t idx : group) {
      if (idx >= buffer.metadata.size())
        continue;
      if (buffer.metadata[idx].hasFill)
        drawFillCommand(buffer.commands[idx]);
    }
    group.clear();
  };

  auto handleBarrier = [&](const CanvasCommand &cmd) {
    if (const auto *save = std::get_if<SaveCommand>(&cmd)) {
      (void)save;
      stack.push_back(currentTransform);
    } else if (const auto *restore = std::get_if<RestoreCommand>(&cmd)) {
      (void)restore;
      if (!stack.empty()) {
        currentTransform = stack.back();
        stack.pop_back();
      }
    } else if (const auto *tf = std::get_if<TransformCommand>(&cmd)) {
      currentTransform = tf->transform;
    } else if (const auto *text = std::get_if<TextCommand>(&cmd)) {
      Viewer2DRenderPoint anchor = mapPoint(text->x, text->y);
      double fontSize = text->style.fontSize * mapping_.scale;
      double lineHeight = fontSize;
      if (text->style.lineHeight > 0.0f)
        lineHeight = text->style.lineHeight * mapping_.scale;
      if (lineHeight <= 0.0)
        lineHeight = fontSize;
      double outline = 0.0;
      if (text->style.outlineWidth > 0.0f)
        outline = text->style.outlineWidth * mapping_.scale;
      Viewer2DRenderText renderText{anchor, text->text, text->style,
                                    fontSize, lineHeight, outline};
      backend_.DrawText(renderText);
    } else if (const auto *instance =
                   std::get_if<SymbolInstanceCommand>(&cmd)) {
      drawSymbolInstance(instance->symbolId, instance->transform);
    }
  };

  for (size_t i = 0; i < buffer.commands.size(); ++i) {
    const auto &cmd = buffer.commands[i];
    bool isBarrier = std::visit(
        [&](auto &&c) {
          using T = std::decay_t<decltype(c)>;
          return std::is_same_v<T, SaveCommand> ||
                 std::is_same_v<T, RestoreCommand> ||
                 std::is_same_v<T, TransformCommand> ||
                 std::is_same_v<T, BeginSymbolCommand> ||
                 std::is_same_v<T, EndSymbolCommand> ||
                 std::is_same_v<T, PlaceSymbolCommand> ||
                 std::is_same_v<T, SymbolInstanceCommand> ||
                 std::is_same_v<T, TextCommand>;
        },
        cmd);

    if (isBarrier) {
      flushGroup();
      handleBarrier(cmd);
      continue;
    }

    if (group.empty())
      currentSource = buffer.sources[i];

    if (buffer.sources[i] != currentSource) {
      flushGroup();
      currentSource = buffer.sources[i];
    }

    group.push_back(i);
  }

  flushGroup();
}

} // namespace viewer2d
