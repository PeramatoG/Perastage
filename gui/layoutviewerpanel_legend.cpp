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
#include "layoutviewerpanel.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <filesystem>
#include <functional>
#include <limits>
#include <map>
#include <vector>

// Include GLEW or other OpenGL loader first if present
#ifdef __APPLE__
#  include <OpenGL/gl.h>
#  include <OpenGL/glu.h>
#else
#  include <GL/gl.h>
#  include <GL/glu.h>
#endif

#include "configmanager.h"
#include "gdtfloader.h"
#include "layoutviewerpanel_shared.h"
#include "legendutils.h"
#include "layouts/LayoutManager.h"
#include "viewer2dcommandrenderer.h"
#include <wx/dcgraph.h>
#include <wx/filename.h>
#include <wx/graphics.h>

namespace {
constexpr double kLegendContentScale = 0.7;
constexpr int kLegendSymbolSizePx =
    static_cast<int>(64 * kLegendContentScale);

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

struct LegendRenderState {
  CanvasTransform current{};
  std::vector<CanvasTransform> stack;
};

struct LegendLocalPoint {
  double x = 0.0;
  double y = 0.0;
};

LegendLocalPoint ApplyLegendTransform(const Transform2D &t, float x, float y) {
  return {t.a * x + t.c * y + t.tx, t.b * x + t.d * y + t.ty};
}

Transform2D ComposeLegendTransform(const Transform2D &a, const Transform2D &b) {
  Transform2D out;
  out.a = a.a * b.a + a.c * b.b;
  out.b = a.b * b.a + a.d * b.b;
  out.c = a.a * b.c + a.c * b.d;
  out.d = a.b * b.c + a.d * b.d;
  out.tx = a.a * b.tx + a.c * b.ty + a.tx;
  out.ty = a.b * b.tx + a.d * b.ty + a.ty;
  return out;
}

viewer2d::Viewer2DRenderPoint MapLegendPoint(
    const Transform2D &localTransform, const CanvasTransform &currentTransform,
    const viewer2d::Viewer2DRenderMapping &mapping, float x, float y) {
  LegendLocalPoint transformed = ApplyLegendTransform(localTransform, x, y);
  double tx = transformed.x * currentTransform.scale + currentTransform.offsetX;
  double ty = transformed.y * currentTransform.scale + currentTransform.offsetY;
  double mappedX = mapping.offsetX + (tx - mapping.minX) * mapping.scale;
  double mappedY = mapping.offsetY + mapping.drawHeight -
                   (ty - mapping.minY) * mapping.scale;
  return viewer2d::Viewer2DRenderPoint{mappedX, mappedY};
}

class LegendSymbolBackend;

void RenderLegendCommandBuffer(
    const CommandBuffer &buffer, const Transform2D &localTransform,
    const SymbolDefinitionSnapshot *symbols, LegendSymbolBackend &backend,
    const viewer2d::Viewer2DRenderMapping &mapping);

void RenderLegendDrawCommand(
    const CanvasCommand &command, const Transform2D &localTransform,
    const CanvasTransform &currentTransform,
    const SymbolDefinitionSnapshot *symbols, LegendSymbolBackend &backend,
    const viewer2d::Viewer2DRenderMapping &mapping, bool drawStrokes,
    bool drawFills);

wxColour ToWxColor(const CanvasColor &color) {
  auto clamp = [](float v) {
    return static_cast<unsigned char>(
        std::clamp(v, 0.0f, 1.0f) * 255.0f);
  };
  return wxColour(clamp(color.r), clamp(color.g), clamp(color.b),
                  clamp(color.a));
}

class LegendSymbolBackend : public viewer2d::IViewer2DCommandBackend {
public:
  explicit LegendSymbolBackend(wxGCDC &dc)
      : dc_(dc), gc_(dc.GetGraphicsContext()) {}

  void SetRenderMode(bool drawStrokes, bool drawFills) {
    drawStrokes_ = drawStrokes;
    drawFills_ = drawFills;
  }

  void SetStrokeScale(double scale) {
    strokeScale_ = scale;
  }

  int StrokeWidthPx(double strokeWidthPx) const {
    strokeWidthPx *= strokeScale_;
    if (strokeWidthPx <= 0.0)
      return 0;
    return std::max(1, static_cast<int>(std::lround(strokeWidthPx)));
  }

  wxPen MakeStrokePen(const CanvasStroke &stroke, double strokeWidthPx) const {
    int strokeWidth = StrokeWidthPx(strokeWidthPx);
    if (strokeWidth <= 0)
      return *wxTRANSPARENT_PEN;
    return wxPen(ToWxColor(stroke.color), strokeWidth);
  }

  wxBrush MakeFillBrush(const CanvasFill *fill) const {
    if (!fill)
      return *wxTRANSPARENT_BRUSH;
    return wxBrush(ToWxColor(fill->color));
  }

  void DrawLine(const viewer2d::Viewer2DRenderPoint &p0,
                const viewer2d::Viewer2DRenderPoint &p1,
                const CanvasStroke &stroke, double strokeWidthPx) override {
    if (!drawStrokes_)
      return;
    wxPen pen = MakeStrokePen(stroke, strokeWidthPx);
    if (pen.GetStyle() == wxPENSTYLE_TRANSPARENT)
      return;
    if (gc_) {
      gc_->SetPen(pen);
      gc_->StrokeLine(p0.x, p0.y, p1.x, p1.y);
      return;
    }
    dc_.SetPen(pen);
    dc_.SetBrush(*wxTRANSPARENT_BRUSH);
    dc_.DrawLine(wxPoint(std::lround(p0.x), std::lround(p0.y)),
                 wxPoint(std::lround(p1.x), std::lround(p1.y)));
  }

  void DrawPolyline(const std::vector<viewer2d::Viewer2DRenderPoint> &points,
                    const CanvasStroke &stroke,
                    double strokeWidthPx) override {
    if (points.empty())
      return;
    if (!drawStrokes_)
      return;
    wxPen pen = MakeStrokePen(stroke, strokeWidthPx);
    if (pen.GetStyle() == wxPENSTYLE_TRANSPARENT)
      return;
    if (gc_) {
      wxGraphicsPath path = gc_->CreatePath();
      path.MoveToPoint(points.front().x, points.front().y);
      for (size_t i = 1; i < points.size(); ++i)
        path.AddLineToPoint(points[i].x, points[i].y);
      gc_->SetPen(pen);
      gc_->StrokePath(path);
      return;
    }
    dc_.SetPen(pen);
    dc_.SetBrush(*wxTRANSPARENT_BRUSH);
    std::vector<wxPoint> wxPoints;
    wxPoints.reserve(points.size());
    for (const auto &pt : points)
      wxPoints.emplace_back(std::lround(pt.x), std::lround(pt.y));
    dc_.DrawLines(static_cast<int>(wxPoints.size()), wxPoints.data());
  }

  void DrawPolygon(const std::vector<viewer2d::Viewer2DRenderPoint> &points,
                   const CanvasStroke &stroke, const CanvasFill *fill,
                   double strokeWidthPx) override {
    if (points.empty())
      return;
    bool shouldFill = drawFills_ && fill != nullptr;
    bool shouldStroke = drawStrokes_;
    wxPen pen = shouldStroke ? MakeStrokePen(stroke, strokeWidthPx)
                             : *wxTRANSPARENT_PEN;
    wxBrush brush =
        shouldFill ? MakeFillBrush(fill) : *wxTRANSPARENT_BRUSH;
    if (gc_) {
      wxGraphicsPath path = gc_->CreatePath();
      path.MoveToPoint(points.front().x, points.front().y);
      for (size_t i = 1; i < points.size(); ++i)
        path.AddLineToPoint(points[i].x, points[i].y);
      path.CloseSubpath();
      gc_->SetBrush(brush);
      gc_->SetPen(pen);
      if (shouldFill && brush.GetStyle() != wxBRUSHSTYLE_TRANSPARENT)
        gc_->FillPath(path);
      if (shouldStroke && pen.GetStyle() != wxPENSTYLE_TRANSPARENT)
        gc_->StrokePath(path);
      return;
    }
    if (!shouldFill && !shouldStroke)
      return;
    dc_.SetPen(pen);
    dc_.SetBrush(brush);
    std::vector<wxPoint> wxPoints;
    wxPoints.reserve(points.size());
    for (const auto &pt : points)
      wxPoints.emplace_back(std::lround(pt.x), std::lround(pt.y));
    dc_.DrawPolygon(static_cast<int>(wxPoints.size()), wxPoints.data());
  }

  void DrawCircle(const viewer2d::Viewer2DRenderPoint &center, double radiusPx,
                  const CanvasStroke &stroke, const CanvasFill *fill,
                  double strokeWidthPx) override {
    bool shouldFill = drawFills_ && fill != nullptr;
    bool shouldStroke = drawStrokes_;
    wxPen pen = shouldStroke ? MakeStrokePen(stroke, strokeWidthPx)
                             : *wxTRANSPARENT_PEN;
    wxBrush brush =
        shouldFill ? MakeFillBrush(fill) : *wxTRANSPARENT_BRUSH;
    if (gc_) {
      if (shouldFill || shouldStroke) {
        gc_->SetBrush(brush);
        gc_->SetPen(pen);
        gc_->DrawEllipse(center.x - radiusPx, center.y - radiusPx,
                         radiusPx * 2.0, radiusPx * 2.0);
      }
      return;
    }
    if (!shouldFill && !shouldStroke)
      return;
    dc_.SetPen(pen);
    dc_.SetBrush(brush);
    dc_.DrawCircle(wxPoint(std::lround(center.x), std::lround(center.y)),
                   std::lround(radiusPx));
  }

  void DrawText(const viewer2d::Viewer2DRenderText &text) override {
    (void)text;
  }

private:
  wxGCDC &dc_;
  wxGraphicsContext *gc_ = nullptr;
  bool drawStrokes_ = true;
  bool drawFills_ = true;
  double strokeScale_ = 1.0;
};

void RenderLegendDrawCommand(
    const CanvasCommand &command, const Transform2D &localTransform,
    const CanvasTransform &currentTransform,
    const SymbolDefinitionSnapshot *symbols, LegendSymbolBackend &backend,
    const viewer2d::Viewer2DRenderMapping &mapping, bool drawStrokes,
    bool drawFills) {
  auto strokeWidth = [&](float width) { return width * mapping.scale; };
  std::visit(
      [&](auto &&cmd) {
        using T = std::decay_t<decltype(cmd)>;
        if constexpr (std::is_same_v<T, LineCommand>) {
          if (!drawStrokes)
            return;
          viewer2d::Viewer2DRenderPoint p0 =
              MapLegendPoint(localTransform, currentTransform, mapping, cmd.x0,
                             cmd.y0);
          viewer2d::Viewer2DRenderPoint p1 =
              MapLegendPoint(localTransform, currentTransform, mapping, cmd.x1,
                             cmd.y1);
          backend.DrawLine(p0, p1, cmd.stroke, strokeWidth(cmd.stroke.width));
        } else if constexpr (std::is_same_v<T, PolylineCommand>) {
          if (!drawStrokes || cmd.points.size() < 4)
            return;
          std::vector<viewer2d::Viewer2DRenderPoint> points;
          points.reserve(cmd.points.size() / 2);
          for (size_t i = 0; i + 1 < cmd.points.size(); i += 2) {
            points.push_back(MapLegendPoint(localTransform, currentTransform,
                                            mapping, cmd.points[i],
                                            cmd.points[i + 1]));
          }
          backend.DrawPolyline(points, cmd.stroke,
                               strokeWidth(cmd.stroke.width));
        } else if constexpr (std::is_same_v<T, PolygonCommand>) {
          if ((!drawStrokes && (!drawFills || !cmd.hasFill)) ||
              cmd.points.size() < 6)
            return;
          std::vector<viewer2d::Viewer2DRenderPoint> points;
          points.reserve(cmd.points.size() / 2);
          for (size_t i = 0; i + 1 < cmd.points.size(); i += 2) {
            points.push_back(MapLegendPoint(localTransform, currentTransform,
                                            mapping, cmd.points[i],
                                            cmd.points[i + 1]));
          }
          const CanvasFill *fill =
              (drawFills && cmd.hasFill) ? &cmd.fill : nullptr;
          backend.DrawPolygon(points, cmd.stroke, fill,
                              strokeWidth(cmd.stroke.width));
        } else if constexpr (std::is_same_v<T, RectangleCommand>) {
          if (!drawStrokes && (!drawFills || !cmd.hasFill))
            return;
          std::vector<float> pts = {
              cmd.x,         cmd.y,         cmd.x + cmd.w, cmd.y,
              cmd.x + cmd.w, cmd.y + cmd.h, cmd.x,         cmd.y + cmd.h};
          std::vector<viewer2d::Viewer2DRenderPoint> points;
          points.reserve(pts.size() / 2);
          for (size_t i = 0; i + 1 < pts.size(); i += 2) {
            points.push_back(MapLegendPoint(localTransform, currentTransform,
                                            mapping, pts[i], pts[i + 1]));
          }
          const CanvasFill *fill =
              (drawFills && cmd.hasFill) ? &cmd.fill : nullptr;
          backend.DrawPolygon(points, cmd.stroke, fill,
                              strokeWidth(cmd.stroke.width));
        } else if constexpr (std::is_same_v<T, CircleCommand>) {
          if (!drawStrokes && (!drawFills || !cmd.hasFill))
            return;
          viewer2d::Viewer2DRenderPoint center =
              MapLegendPoint(localTransform, currentTransform, mapping, cmd.cx,
                             cmd.cy);
          float sx = std::sqrt(localTransform.a * localTransform.a +
                               localTransform.b * localTransform.b);
          float sy = std::sqrt(localTransform.c * localTransform.c +
                               localTransform.d * localTransform.d);
          float scale = (sx + sy) * 0.5f;
          double radius =
              cmd.radius * scale * currentTransform.scale * mapping.scale;
          const CanvasFill *fill =
              (drawFills && cmd.hasFill) ? &cmd.fill : nullptr;
          backend.DrawCircle(center, radius, cmd.stroke, fill,
                             strokeWidth(cmd.stroke.width));
        } else if constexpr (std::is_same_v<T, SymbolInstanceCommand>) {
          if (!symbols)
            return;
          auto it = symbols->find(cmd.symbolId);
          if (it == symbols->end())
            return;
          Transform2D combined = ComposeLegendTransform(localTransform,
                                                       cmd.transform);
          RenderLegendCommandBuffer(it->second.localCommands, combined, symbols,
                                    backend, mapping);
        } else {
          (void)cmd;
        }
      },
      command);
}

void RenderLegendCommandBuffer(
    const CommandBuffer &buffer, const Transform2D &localTransform,
    const SymbolDefinitionSnapshot *symbols, LegendSymbolBackend &backend,
    const viewer2d::Viewer2DRenderMapping &mapping) {
  LegendRenderState state{};
  std::vector<size_t> group;
  std::string currentSource;

  auto hasStroke = [&](size_t idx) {
    return idx < buffer.metadata.size() ? buffer.metadata[idx].hasStroke : true;
  };
  auto hasFill = [&](size_t idx) {
    return idx < buffer.metadata.size() ? buffer.metadata[idx].hasFill : true;
  };

  auto flushGroup = [&]() {
    if (group.empty())
      return;
    backend.SetRenderMode(true, false);
    for (size_t idx : group) {
      if (!hasStroke(idx))
        continue;
      RenderLegendDrawCommand(buffer.commands[idx], localTransform,
                              state.current, symbols, backend, mapping, true,
                              false);
    }
    backend.SetRenderMode(false, true);
    for (size_t idx : group) {
      if (!hasFill(idx))
        continue;
      RenderLegendDrawCommand(buffer.commands[idx], localTransform,
                              state.current, symbols, backend, mapping, false,
                              true);
    }
    group.clear();
  };

  auto handleBarrier = [&](const auto &cmd) {
    using T = std::decay_t<decltype(cmd)>;
    if constexpr (std::is_same_v<T, SaveCommand>) {
      state.stack.push_back(state.current);
    } else if constexpr (std::is_same_v<T, RestoreCommand>) {
      if (!state.stack.empty()) {
        state.current = state.stack.back();
        state.stack.pop_back();
      }
    } else if constexpr (std::is_same_v<T, TransformCommand>) {
      state.current = cmd.transform;
    } else if constexpr (std::is_same_v<T, SymbolInstanceCommand>) {
      RenderLegendDrawCommand(cmd, localTransform, state.current, symbols,
                              backend, mapping, true, true);
    } else {
      (void)cmd;
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
      std::visit([&](const auto &barrierCmd) { handleBarrier(barrierCmd); },
                 cmd);
      continue;
    }

    if (group.empty() && i < buffer.sources.size()) {
      currentSource = buffer.sources[i];
    }

    if (i < buffer.sources.size() && buffer.sources[i] != currentSource) {
      flushGroup();
      currentSource = buffer.sources[i];
    }

    group.push_back(i);
  }

  flushGroup();
}
} // namespace

layouts::LayoutLegendDefinition *LayoutViewerPanel::GetSelectedLegend() {
  if (currentLayout.legendViews.empty())
    return nullptr;
  if (selectedElementType == SelectedElementType::Legend &&
      selectedElementId >= 0) {
    for (auto &legend : currentLayout.legendViews) {
      if (legend.id == selectedElementId)
        return &legend;
    }
  }
  selectedElementType = SelectedElementType::Legend;
  selectedElementId = currentLayout.legendViews.front().id;
  return &currentLayout.legendViews.front();
}

const layouts::LayoutLegendDefinition *LayoutViewerPanel::GetSelectedLegend()
    const {
  if (currentLayout.legendViews.empty())
    return nullptr;
  if (selectedElementType == SelectedElementType::Legend &&
      selectedElementId >= 0) {
    for (const auto &legend : currentLayout.legendViews) {
      if (legend.id == selectedElementId)
        return &legend;
    }
  }
  if (!currentLayout.legendViews.empty())
    return &currentLayout.legendViews.front();
  return nullptr;
}

bool LayoutViewerPanel::GetLegendFrameById(
    int legendId, layouts::Layout2DViewFrame &frame) const {
  if (legendId <= 0)
    return false;
  for (const auto &legend : currentLayout.legendViews) {
    if (legend.id == legendId) {
      frame = legend.frame;
      return true;
    }
  }
  return false;
}

void LayoutViewerPanel::UpdateLegendFrame(const layouts::Layout2DViewFrame &frame,
                                          bool updatePosition) {
  layouts::LayoutLegendDefinition *legend = GetSelectedLegend();
  if (!legend)
    return;
  legend->frame.width = frame.width;
  legend->frame.height = frame.height;
  if (updatePosition) {
    legend->frame.x = frame.x;
    legend->frame.y = frame.y;
  }
  if (!currentLayout.name.empty()) {
    layouts::LayoutManager::Get().UpdateLayoutLegend(currentLayout.name,
                                                     *legend);
  }
  InvalidateRenderIfFrameChanged();
  if (NeedsRenderRebuild()) {
    RequestRenderRebuild();
  }
  Refresh();
}

void LayoutViewerPanel::OnDeleteLegend(wxCommandEvent &) {
  if (selectedElementType != SelectedElementType::Legend)
    return;
  const layouts::LayoutLegendDefinition *legend = GetSelectedLegend();
  if (!legend)
    return;
  const int legendId = legend->id;
  if (!currentLayout.name.empty()) {
    if (layouts::LayoutManager::Get().RemoveLayoutLegend(currentLayout.name,
                                                        legendId)) {
      auto &legends = currentLayout.legendViews;
      legends.erase(std::remove_if(legends.begin(), legends.end(),
                                   [legendId](const auto &entry) {
                                     return entry.id == legendId;
                                   }),
                    legends.end());
      if (selectedElementId == legendId) {
        if (!currentLayout.view2dViews.empty()) {
          selectedElementType = SelectedElementType::View2D;
          selectedElementId = currentLayout.view2dViews.front().id;
        } else if (!legends.empty()) {
          selectedElementType = SelectedElementType::Legend;
          selectedElementId = legends.front().id;
        } else if (!currentLayout.textViews.empty()) {
          selectedElementType = SelectedElementType::Text;
          selectedElementId = currentLayout.textViews.front().id;
        } else if (!currentLayout.eventTables.empty()) {
          selectedElementType = SelectedElementType::EventTable;
          selectedElementId = currentLayout.eventTables.front().id;
        } else if (!currentLayout.imageViews.empty()) {
          selectedElementType = SelectedElementType::Image;
          selectedElementId = currentLayout.imageViews.front().id;
        } else {
          selectedElementType = SelectedElementType::None;
          selectedElementId = -1;
        }
      }
    }
  }
  auto cacheIt = legendCaches_.find(legendId);
  if (cacheIt != legendCaches_.end()) {
    ClearCachedTexture(cacheIt->second);
    legendCaches_.erase(cacheIt);
  }
  Refresh();
}

void LayoutViewerPanel::DrawLegendElement(
    const layouts::LayoutLegendDefinition &legend, int activeLegendId) {
  LegendCache &cache = GetLegendCache(legend.id);
  wxRect frameRect;
  if (!GetFrameRect(legend.frame, frameRect))
    return;
  const int frameRight = frameRect.GetLeft() + frameRect.GetWidth();
  const int frameBottom = frameRect.GetTop() + frameRect.GetHeight();

  const wxSize renderSize = GetFrameSizeForZoom(legend.frame, cache.renderZoom);
  if (cache.texture != 0 && renderSize.GetWidth() > 0 &&
      renderSize.GetHeight() > 0 && cache.textureSize == renderSize) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, cache.texture);
    glColor4ub(255, 255, 255, 255);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(static_cast<float>(frameRect.GetLeft()),
               static_cast<float>(frameRect.GetTop()));
    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(static_cast<float>(frameRight),
               static_cast<float>(frameRect.GetTop()));
    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(static_cast<float>(frameRight),
               static_cast<float>(frameBottom));
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(static_cast<float>(frameRect.GetLeft()),
               static_cast<float>(frameRect.GetBottom()));
    glEnd();
    glDisable(GL_TEXTURE_2D);
  } else {
    glColor4ub(245, 245, 245, 255);
    glBegin(GL_QUADS);
    glVertex2f(static_cast<float>(frameRect.GetLeft()),
               static_cast<float>(frameRect.GetTop()));
    glVertex2f(static_cast<float>(frameRect.GetRight()),
               static_cast<float>(frameRect.GetTop()));
    glVertex2f(static_cast<float>(frameRight),
               static_cast<float>(frameBottom));
    glVertex2f(static_cast<float>(frameRect.GetLeft()),
               static_cast<float>(frameRect.GetBottom()));
    glEnd();
  }

  if (legend.id == activeLegendId) {
    glColor4ub(60, 160, 240, 255);
    glLineWidth(2.0f);
  } else {
    glColor4ub(160, 160, 160, 255);
    glLineWidth(1.0f);
  }
  glBegin(GL_LINE_LOOP);
  glVertex2f(static_cast<float>(frameRect.GetLeft()),
             static_cast<float>(frameRect.GetTop()));
  glVertex2f(static_cast<float>(frameRight),
             static_cast<float>(frameRect.GetTop()));
  glVertex2f(static_cast<float>(frameRight),
             static_cast<float>(frameBottom));
  glVertex2f(static_cast<float>(frameRect.GetLeft()),
             static_cast<float>(frameRect.GetBottom()));
  glEnd();

  if (legend.id == activeLegendId)
    DrawSelectionHandles(frameRect);
}

void LayoutViewerPanel::RefreshLegendData() {
  if (currentLayout.legendViews.empty())
    return;
  std::vector<LegendItem> items = BuildLegendItems();
  size_t newHash = HashLegendItems(items);
  if (newHash == legendDataHash)
    return;
  legendItems_ = std::move(items);
  legendDataHash = newHash;
  if (legendItems_.size() == 1 &&
      legendItems_.front().typeName == "No fixtures") {
    return;
  }
  for (auto &entry : legendCaches_) {
    entry.second.renderDirty = true;
  }
  renderDirty = true;
  RequestRenderRebuild();
}

std::vector<LayoutViewerPanel::LegendItem>
LayoutViewerPanel::BuildLegendItems() const {
  struct LegendAggregate {
    int count = 0;
    std::optional<int> channelCount;
    bool mixedChannels = false;
    std::string symbolKey;
    bool mixedSymbols = false;
  };

  std::map<std::string, LegendAggregate> aggregates;
  const auto &fixtures = ConfigManager::Get().GetScene().fixtures;
  const std::string &basePath = ConfigManager::Get().GetScene().basePath;
  for (const auto &[uuid, fixture] : fixtures) {
    (void)uuid;
    std::string typeName = fixture.typeName;
    std::string fullPath;
    if (!fixture.gdtfSpec.empty()) {
      try {
        std::filesystem::path p =
            basePath.empty()
                ? std::filesystem::u8path(fixture.gdtfSpec)
                : std::filesystem::u8path(basePath) /
                      std::filesystem::u8path(fixture.gdtfSpec);
        fullPath = p.string();
      } catch (const std::exception &) {
        fullPath.clear();
      }
    }
    if (typeName.empty() && !fullPath.empty()) {
      wxFileName fn(fullPath);
      typeName = fn.GetFullName().ToStdString();
    }
    if (typeName.empty())
      typeName = "Unknown";

    int chCount = GetGdtfModeChannelCount(fullPath, fixture.gdtfMode);
    const std::string symbolKey = BuildFixtureSymbolKey(fixture, basePath);
    LegendAggregate &agg = aggregates[typeName];
    agg.count += 1;
    if (chCount >= 0) {
      if (!agg.channelCount.has_value()) {
        agg.channelCount = chCount;
      } else if (agg.channelCount.value() != chCount) {
        agg.mixedChannels = true;
      }
    }
    if (!symbolKey.empty()) {
      if (agg.symbolKey.empty()) {
        agg.symbolKey = symbolKey;
      } else if (agg.symbolKey != symbolKey) {
        agg.mixedSymbols = true;
      }
    }
  }

  std::vector<LegendItem> items;
  items.reserve(aggregates.size());
  for (const auto &[typeName, agg] : aggregates) {
    LegendItem item;
    item.typeName = typeName;
    item.count = agg.count;
    if (agg.channelCount.has_value() && !agg.mixedChannels)
      item.channelCount = agg.channelCount;
    if (!agg.mixedSymbols)
      item.symbolKey = agg.symbolKey;
    items.push_back(item);
  }

  if (items.empty()) {
    LegendItem item;
    item.typeName = "No fixtures";
    item.count = 0;
    items.push_back(item);
  }

  return items;
}

size_t LayoutViewerPanel::HashLegendItems(
    const std::vector<LegendItem> &items) const {
  size_t hash = items.size();
  std::hash<std::string> strHasher;
  std::hash<int> intHasher;
  for (const auto &item : items) {
    hash ^= strHasher(item.typeName) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= intHasher(item.count) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    int chValue = item.channelCount.value_or(-1);
    hash ^= intHasher(chValue) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= strHasher(item.symbolKey) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
  }
  return hash;
}

wxImage LayoutViewerPanel::BuildLegendImage(
    const wxSize &size, const wxSize &logicalSize, double renderZoom,
    const std::vector<LegendItem> &items,
    const SymbolDefinitionSnapshot *symbols) const {
  if (size.GetWidth() <= 0 || size.GetHeight() <= 0 || renderZoom <= 0.0)
    return wxImage();
  wxBitmap bitmap(size.GetWidth(), size.GetHeight(), 32);
  wxMemoryDC memoryDc(bitmap);
  wxGCDC dc(memoryDc);
  dc.SetBackground(wxBrush(wxColour(255, 255, 255)));
  dc.Clear();
  dc.SetTextForeground(wxColour(20, 20, 20));
  dc.SetPen(*wxTRANSPARENT_PEN);

  const int paddingLeft = 4;
  const int paddingRight = 4;
  const int paddingTop = 6;
  const int paddingBottom = 2;
  const int columnGap = 8;
  const int symbolColumnGap = 2;
  constexpr double kLegendLineSpacingScale = 1.0;
  constexpr double kLegendSymbolColumnScale = 1.0;
  constexpr double kLegendSymbolPairOverlapScale = 0.5;
  const int totalRows = static_cast<int>(items.size()) + 1;
  const int baseHeight = logicalSize.GetHeight() > 0 ? logicalSize.GetHeight()
                                                     : size.GetHeight();
  const double separatorGap = 2.0;
  const double availableHeight = static_cast<double>(baseHeight) -
                                 paddingTop - paddingBottom - separatorGap;
  double fontSize =
      totalRows > 0 ? (static_cast<double>(availableHeight) / totalRows) - 2.0
                    : 10.0;
  fontSize = std::clamp(fontSize, 6.0, 14.0);
  fontSize *= kLegendFontScale;
  const double fontScale =
      std::clamp(fontSize / (14.0 * kLegendFontScale), 0.0, 1.0);
  const int fontSizePx =
      std::max(1, static_cast<int>(std::lround(fontSize * renderZoom)));

  wxFont baseFont =
      layoutviewerpanel::detail::MakeSharedFont(fontSizePx,
                                                wxFONTWEIGHT_NORMAL);
  wxFont headerFont =
      layoutviewerpanel::detail::MakeSharedFont(fontSizePx,
                                                wxFONTWEIGHT_BOLD);

  auto measureTextWidth = [&](const wxString &text) {
    int w = 0;
    int h = 0;
    dc.GetTextExtent(text, &w, &h);
    return w;
  };

  dc.SetFont(baseFont);
  int maxCountWidth = measureTextWidth("Count");
  int maxChWidth = measureTextWidth("Ch");
  for (const auto &item : items) {
    maxCountWidth = std::max(
        maxCountWidth, measureTextWidth(wxString::Format("%d", item.count)));
    wxString chText = item.channelCount.has_value()
                          ? wxString::Format("%d", item.channelCount.value())
                          : wxString("-");
    maxChWidth = std::max(maxChWidth, measureTextWidth(chText));
  }
  const int leftTrimPx = measureTextWidth("000");
  const int chExtraWidthPx = measureTextWidth("0");
  maxChWidth += chExtraWidthPx;

  int textHeight = 0;
  int lineWidth = 0;
  dc.GetTextExtent("Hg", &lineWidth, &textHeight);
  const int separatorGapPx =
      std::max(1, static_cast<int>(std::lround(separatorGap * renderZoom)));
  const int lineHeight = textHeight + separatorGapPx;

  const double rowHeight = totalRows > 0 ? availableHeight / totalRows : 0.0;
  const int baseRowHeightPx =
      std::max(lineHeight,
               static_cast<int>(std::lround(rowHeight * renderZoom *
                                            kLegendLineSpacingScale)));
  const int desiredSymbolSize = static_cast<int>(std::lround(
      kLegendSymbolSizePx * renderZoom * fontScale));
  const int symbolSize = std::max(4, desiredSymbolSize);
  const double symbolPairGapPx =
      -std::max(1.0, static_cast<double>(symbolSize) *
                         kLegendSymbolPairOverlapScale);
  auto symbolDrawWidth = [&](const SymbolDefinition *symbol) -> double {
    if (!symbol)
      return 0.0;
    const float symbolW = symbol->bounds.max.x - symbol->bounds.min.x;
    const float symbolH = symbol->bounds.max.y - symbol->bounds.min.y;
    if (symbolW <= 0.0f || symbolH <= 0.0f)
      return 0.0;
    double scale =
        std::min(static_cast<double>(symbolSize) / symbolW,
                 static_cast<double>(symbolSize) / symbolH);
    return symbolW * scale;
  };
  auto symbolDrawHeight = [&](const SymbolDefinition *symbol) -> double {
    if (!symbol)
      return 0.0;
    const float symbolW = symbol->bounds.max.x - symbol->bounds.min.x;
    const float symbolH = symbol->bounds.max.y - symbol->bounds.min.y;
    if (symbolW <= 0.0f || symbolH <= 0.0f)
      return 0.0;
    double scale =
        std::min(static_cast<double>(symbolSize) / symbolW,
                 static_cast<double>(symbolSize) / symbolH);
    return symbolH * scale;
  };
  double maxSymbolPairWidth = symbolSize;
  if (symbols) {
    for (const auto &item : items) {
      if (item.symbolKey.empty())
        continue;
      const SymbolDefinition *topSymbol = FindSymbolDefinitionPreferred(
          symbols, item.symbolKey, SymbolViewKind::Top);
      const SymbolDefinition *frontSymbol = FindSymbolDefinitionExact(
          symbols, item.symbolKey, SymbolViewKind::Front);
      const double topDrawW = symbolDrawWidth(topSymbol);
      const double frontDrawW = symbolDrawWidth(frontSymbol);
      double rowPairWidth = std::max(topDrawW, frontDrawW);
      if (topDrawW > 0.0 && frontDrawW > 0.0)
        rowPairWidth = topDrawW + frontDrawW + symbolPairGapPx;
      maxSymbolPairWidth = std::max(maxSymbolPairWidth, rowPairWidth);
    }
  }
  const int symbolSlotSize = std::max(
      4, static_cast<int>(std::ceil(maxSymbolPairWidth *
                                    kLegendSymbolColumnScale)));
  const int rowHeightPx = baseRowHeightPx;
  const int paddingLeftPx =
      std::max(0, static_cast<int>(std::lround(paddingLeft * renderZoom)));
  const int paddingRightPx =
      std::max(0, static_cast<int>(std::lround(paddingRight * renderZoom)));
  const int paddingTopPx =
      std::max(0, static_cast<int>(std::lround(paddingTop * renderZoom)));
  const int paddingBottomPx =
      std::max(0, static_cast<int>(std::lround(paddingBottom * renderZoom)));
  const int columnGapPx =
      std::max(0, static_cast<int>(std::lround(columnGap * renderZoom)));
  const int symbolColumnGapPx =
      std::max(0, static_cast<int>(std::lround(symbolColumnGap * renderZoom)));
  int xSymbol = paddingLeftPx - leftTrimPx;
  int xCount = xSymbol + symbolSlotSize + symbolColumnGapPx;
  int xType = xCount + maxCountWidth + columnGapPx;
  int xCh = size.GetWidth() - paddingRightPx - maxChWidth;
  if (xCh < xType + columnGapPx)
    xCh = xType + columnGapPx;
  int typeWidth = std::max(0, xCh - xType - columnGapPx);

  auto trimTextToWidth = [&](const wxString &text, int maxWidth) {
    if (maxWidth <= 0)
      return wxString();
    int textWidth = measureTextWidth(text);
    if (textWidth <= maxWidth)
      return text;
    wxString ellipsis = "...";
    int ellipsisWidth = measureTextWidth(ellipsis);
    if (ellipsisWidth >= maxWidth)
      return ellipsis.Left(1);
    wxString trimmed = text;
    while (!trimmed.empty() &&
           measureTextWidth(trimmed) + ellipsisWidth > maxWidth) {
      trimmed.RemoveLast();
    }
    return trimmed + ellipsis;
  };

  int y = paddingTopPx;
  const int textOffset = std::max(0, (rowHeightPx - textHeight) / 2);
  dc.SetFont(headerFont);
  dc.DrawText("Count", xCount, y + textOffset);
  dc.DrawText("Type", xType, y + textOffset);
  dc.DrawText("Ch", xCh, y + textOffset);

  y += rowHeightPx;
  dc.SetPen(wxPen(wxColour(200, 200, 200)));
  dc.DrawLine(xSymbol, y, size.GetWidth() - paddingRightPx, y);
  y += separatorGapPx;

  dc.SetFont(baseFont);
  LegendSymbolBackend backend(dc);
  for (const auto &item : items) {
    if (y + rowHeightPx > size.GetHeight() - paddingBottomPx)
      break;
    wxString countText = wxString::Format("%d", item.count);
    wxString typeText =
        trimTextToWidth(wxString::FromUTF8(item.typeName), typeWidth);
    wxString chText = item.channelCount.has_value()
                          ? wxString::Format("%d", item.channelCount.value())
                          : wxString("-");
    if (symbols && !item.symbolKey.empty()) {
      const SymbolDefinition *topSymbol = FindSymbolDefinitionPreferred(
          symbols, item.symbolKey, SymbolViewKind::Top);
      const SymbolDefinition *frontSymbol = FindSymbolDefinitionExact(
          symbols, item.symbolKey, SymbolViewKind::Front);
      auto drawSymbol = [&](const SymbolDefinition *symbol, double drawLeft,
                            double drawTop) {
        if (!symbol)
          return;
        const float symbolW = symbol->bounds.max.x - symbol->bounds.min.x;
        const float symbolH = symbol->bounds.max.y - symbol->bounds.min.y;
        if (symbolW <= 0.0f || symbolH <= 0.0f)
          return;
        double scale =
            std::min(static_cast<double>(symbolSize) / symbolW,
                     static_cast<double>(symbolSize) / symbolH);
        double drawH = symbolH * scale;
        viewer2d::Viewer2DRenderMapping mapping{};
        mapping.minX = symbol->bounds.min.x;
        mapping.minY = symbol->bounds.min.y;
        mapping.scale = scale;
        mapping.offsetX = drawLeft;
        mapping.offsetY = drawTop;
        mapping.drawHeight = drawH;
        backend.SetStrokeScale(
            mapping.scale > 0.0 ? 1.0 / mapping.scale : 1.0);
        RenderLegendCommandBuffer(symbol->localCommands,
                                  Transform2D::Identity(), symbols, backend,
                                  mapping);
      };
      const double topDrawW = symbolDrawWidth(topSymbol);
      const double frontDrawW = symbolDrawWidth(frontSymbol);
      const double topDrawH = symbolDrawHeight(topSymbol);
      const double frontDrawH = symbolDrawHeight(frontSymbol);
      if (topDrawW > 0.0 || frontDrawW > 0.0) {
        const double slotWidth = static_cast<double>(symbolSlotSize);
        double rowPairWidth = std::max(topDrawW, frontDrawW);
        if (topDrawW > 0.0 && frontDrawW > 0.0)
          rowPairWidth = topDrawW + frontDrawW + symbolPairGapPx;
        const double rowStart =
            xSymbol + std::max(0.0, (slotWidth - rowPairWidth) * 0.5);
        double leftSlotWidth = rowPairWidth;
        double rightSlotWidth = rowPairWidth;
        double topSlotLeft = rowStart;
        double frontSlotLeft = rowStart;
        if (topDrawW > 0.0 && frontDrawW > 0.0) {
          leftSlotWidth = topDrawW;
          rightSlotWidth = frontDrawW;
          frontSlotLeft = rowStart + topDrawW + symbolPairGapPx;
        } else if (frontDrawW > 0.0) {
          frontSlotLeft = rowStart;
        }
        if (topDrawW > 0.0) {
          double symbolDrawTop =
              y + (static_cast<double>(rowHeightPx) - topDrawH) * 0.5;
          double symbolDrawLeft =
              topSlotLeft + std::max(0.0, (leftSlotWidth - topDrawW) * 0.5);
          drawSymbol(topSymbol, symbolDrawLeft, symbolDrawTop);
        }
        if (frontDrawW > 0.0) {
          double symbolDrawTop =
              y + (static_cast<double>(rowHeightPx) - frontDrawH) * 0.5;
          double symbolDrawLeft =
              frontSlotLeft +
              std::max(0.0, (rightSlotWidth - frontDrawW) * 0.5);
          drawSymbol(frontSymbol, symbolDrawLeft, symbolDrawTop);
        }
      }
    }
    dc.DrawText(countText, xCount, y + textOffset);
    dc.DrawText(typeText, xType, y + textOffset);
    dc.DrawText(chText, xCh, y + textOffset);
    y += rowHeightPx;
  }

  memoryDc.SelectObject(wxNullBitmap);
  return bitmap.ConvertToImage();
}
