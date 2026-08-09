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
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Include GLEW or other OpenGL loader first if present
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#ifdef __APPLE__
#  include <OpenGL/gl.h>
#  include <OpenGL/glu.h>
#else
#  include <GL/gl.h>
#  include <GL/glu.h>
#endif

#include "layoutviewerpanel_shared.h"
#include "layoutlegendeditdialog.h"
#include "layoutlegenditems.h"
#include "mainwindow.h"
#include "LayoutManager.h"
#include "guiconfigservices.h"
#include "configmanager.h"
#include "symbols/PerastageSvgSymbol.h"
#include "symbols/fixture_symbol_svg_cache.h"
#include "symbols/fixture_symbol_availability.h"
#include "viewer2dcommandrenderer.h"
#include <wx/dcgraph.h>
#include <wx/graphics.h>

namespace {
constexpr double kLegendContentScale = 0.7;
constexpr int kLegendSymbolSizePx =
    static_cast<int>(64 * kLegendContentScale);
constexpr double kLegendFallbackSymbolScale = 2.0;
constexpr double kLegendSvgSymbolScale = 0.4;
constexpr double kLegendMaxSymbolSlotScale = 1.45;

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

wxColour ResolveLegendSvgFillColor(const std::optional<std::string> &hexColor) {
  if (!hexColor.has_value())
    return wxColour(224, 224, 224);

  wxColour parsed(wxString::FromUTF8(hexColor.value()));
  if (!parsed.IsOk())
    return wxColour(224, 224, 224);
  return parsed;
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

// Updates legend geometry and invalidates only the legend cache when resizing completes.
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
  if (updatePosition) {
    pendingFrameCommit_ = true;
    Refresh();
    return;
  }
  if (!currentLayout.name.empty()) {
    layouts::LayoutManager::Get().UpdateLayoutLegend(currentLayout.name,
                                                     *legend);
  }
  LegendCache &cache = GetLegendCache(legend->id);
  cache.renderDirty = true;
  renderDirty = true;
  RequestRenderRebuild();
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
    auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
    cfg.PushUndoState("delete layout legend");
    if (layouts::LayoutManager::Get().RemoveLayoutLegend(currentLayout.name,
                                                        legendId)) {
      auto &legends = currentLayout.legendViews;
      legends.erase(std::remove_if(legends.begin(), legends.end(),
                                   [legendId](const auto &entry) {
                                     return entry.id == legendId;
                                   }),
                    legends.end());
      InvalidateSelectionIndexCache();
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

void LayoutViewerPanel::OnEditLegend(wxCommandEvent &) {
  if (selectedElementType != SelectedElementType::Legend)
    return;
  layouts::LayoutLegendDefinition *legend = GetSelectedLegend();
  if (!legend)
    return;

  const std::vector<SharedLayoutLegendItem> availableItems =
      BuildSharedLayoutLegendItems();
  LayoutLegendEditDialog dialog(this, *legend, availableItems);
  if (dialog.ShowModal() != wxID_OK)
    return;

  auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  cfg.PushUndoState("edit layout legend");
  legend->showChannelColumn = dialog.GetShowChannelColumn();
  legend->itemSettings = dialog.GetItemSettings();
  layouts::LayoutManager::Get().UpdateLayoutLegend(currentLayout.name, *legend);
  legendDataDirty_ = true;
  RefreshLegendData();
  RequestRenderRebuild();
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
  if (ShouldDeferMissingElementTexture(cache.renderDirty, cache.texture,
                                       cache.textureSize, renderSize)) {
    return;
  }
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

// Rebuilds cached legend items and marks legend textures dirty when legend content changes.
void LayoutViewerPanel::RefreshLegendData() {
  if (auto *mw = MainWindow::Instance();
      mw && mw->IsMvrImportPipelineActive()) {
    return;
  }
  if (!legendDataDirty_)
    return;
  if (currentLayout.legendViews.empty()) {
    legendItems_.clear();
    legendDataHash = 0;
    legendDataDirty_ = false;
    return;
  }
  std::vector<LegendItem> items = BuildLegendItems();
  size_t newHash = HashLegendItems(items);
  if (newHash == legendDataHash) {
    legendDataDirty_ = false;
    return;
  }
  legendItems_ = std::move(items);
  legendDataHash = newHash;
  legendDataDirty_ = false;
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
  std::vector<SharedLayoutLegendItem> sharedItems =
      BuildSharedLayoutLegendItems();
  const layouts::LayoutLegendDefinition *legend = GetSelectedLegend();
  std::unordered_map<std::string, layouts::LayoutLegendDefinition::ItemSettings>
      settingsByType;
  if (legend) {
    settingsByType.reserve(legend->itemSettings.size());
    for (const auto &settings : legend->itemSettings)
      settingsByType[settings.typeName] = settings;
  }

  std::unordered_map<std::string, SharedLayoutLegendItem> availableByType;
  availableByType.reserve(sharedItems.size());
  for (const auto &shared : sharedItems)
    availableByType.emplace(shared.typeName, shared);

  std::vector<LegendItem> items;
  items.reserve(sharedItems.size());
  auto appendItem = [&](const SharedLayoutLegendItem &shared) {
    LegendItem item;
    item.typeName = shared.typeName;
    item.displayName = shared.typeName;
    item.count = shared.count;
    item.channelCount = shared.channelCount;
    item.symbolKey = shared.symbolKey;
    item.gdtfPath = shared.gdtfPath;
    item.symbolFillHex = shared.symbolFillHex;
    if (const auto it = settingsByType.find(shared.typeName);
        it != settingsByType.end()) {
      item.showBottomSymbol = it->second.showBottomSymbol;
      item.showFrontSymbol = it->second.showFrontSymbol;
      item.showSideSymbol = it->second.showSideSymbol;
      if (!it->second.customName.empty())
        item.displayName = it->second.customName;
      if (!it->second.visible)
        return;
    }
    items.push_back(std::move(item));
  };

  if (legend) {
    std::unordered_set<std::string> usedTypes;
    usedTypes.reserve(legend->itemSettings.size());
    for (const auto &settings : legend->itemSettings) {
      const auto it = availableByType.find(settings.typeName);
      if (it == availableByType.end())
        continue;
      appendItem(it->second);
      usedTypes.insert(settings.typeName);
    }
    for (const auto &shared : sharedItems) {
      if (usedTypes.find(shared.typeName) != usedTypes.end())
        continue;
      appendItem(shared);
    }
  } else {
    for (const auto &shared : sharedItems)
      appendItem(shared);
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
    hash ^= strHasher(item.displayName) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= intHasher(item.count) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    int chValue = item.channelCount.value_or(-1);
    hash ^= intHasher(chValue) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= strHasher(item.symbolKey) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= strHasher(item.symbolFillHex.value_or("")) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= intHasher(item.showBottomSymbol ? 1 : 0) + 0x9e3779b9 + (hash << 6) +
            (hash >> 2);
    hash ^= intHasher(item.showFrontSymbol ? 1 : 0) + 0x9e3779b9 + (hash << 6) +
            (hash >> 2);
    hash ^= intHasher(item.showSideSymbol ? 1 : 0) + 0x9e3779b9 + (hash << 6) +
            (hash >> 2);
  }
  const auto *legend = GetSelectedLegend();
  hash ^= intHasher((legend && legend->showChannelColumn) ? 1 : 0) +
          0x9e3779b9 + (hash << 6) + (hash >> 2);
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

  const int paddingLeft = 0;
  const int paddingRight = 0;
  const int paddingTop = 0;
  const int paddingBottom = 0;
  const int columnGap = 6;
  const int symbolColumnGap = 0;
  constexpr double kLegendLineSpacingScale = 1.0;
  constexpr double kLegendSymbolColumnScale = 1.0;
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
  int fontSizePx =
      std::max(1, static_cast<int>(std::lround(fontSize * renderZoom)));

  wxFont baseFont =
      layoutviewerpanel::detail::MakeSharedFont(fontSizePx,
                                                wxFONTWEIGHT_NORMAL);
  wxFont headerFont =
      layoutviewerpanel::detail::MakeSharedFont(fontSizePx,
                                                wxFONTWEIGHT_BOLD);

  std::unordered_map<wxString, wxSize, wxStringHash, wxStringEqual>
      baseTextExtentCache;
  auto measureTextExtent =
      [&](const wxString &text,
          std::unordered_map<wxString, wxSize, wxStringHash, wxStringEqual>
              &cache) {
        auto cacheIt = cache.find(text);
        if (cacheIt != cache.end())
          return cacheIt->second;
        int width = 0;
        int height = 0;
        dc.GetTextExtent(text, &width, &height);
        wxSize sizeMeasured(width, height);
        cache.emplace(text, sizeMeasured);
        return sizeMeasured;
      };

  auto measureTextWidth = [&](const wxString &text) {
    return measureTextExtent(text, baseTextExtentCache).GetWidth();
  };

  struct LegendRowText {
    wxString countText;
    wxString typeText;
    wxString chText;
  };
  std::vector<LegendRowText> rowTexts;
  rowTexts.reserve(items.size());
  for (const auto &item : items) {
    rowTexts.push_back({
        wxString::Format("%d", item.count),
        wxString::FromUTF8(item.displayName),
        item.channelCount.has_value()
            ? wxString::Format("%d", item.channelCount.value())
            : wxString("-")});
  }
  const bool showChannelColumn =
      GetSelectedLegend() ? GetSelectedLegend()->showChannelColumn : true;

  const int paddingLeftPx =
      std::max(0, static_cast<int>(std::lround(paddingLeft * renderZoom)));
  const int paddingRightPx =
      std::max(0, static_cast<int>(std::lround(paddingRight * renderZoom)));
  const int paddingTopPx =
      std::max(0, static_cast<int>(std::lround(paddingTop * renderZoom)));
  const int paddingBottomPx =
      std::max(0, static_cast<int>(std::lround(paddingBottom * renderZoom)));
  const int separatorGapPx =
      std::max(1, static_cast<int>(std::lround(separatorGap * renderZoom)));
  const int contentHeightPx =
      std::max(1, size.GetHeight() - paddingTopPx - paddingBottomPx - separatorGapPx);
  const int maxRowHeightPx =
      std::max(1, totalRows > 0 ? contentHeightPx / totalRows : contentHeightPx);

  int textHeight = 0;
  int lineWidth = 0;
  for (;;) {
    dc.SetFont(baseFont);
    dc.GetTextExtent("Hg", &lineWidth, &textHeight);
    if (textHeight <= maxRowHeightPx || fontSizePx <= 1)
      break;
    --fontSizePx;
    baseFont = layoutviewerpanel::detail::MakeSharedFont(fontSizePx,
                                                          wxFONTWEIGHT_NORMAL);
    headerFont = layoutviewerpanel::detail::MakeSharedFont(fontSizePx,
                                                            wxFONTWEIGHT_BOLD);
  }

  constexpr int kLegendTypeLineCount = 2;
  const int lineHeight =
      (textHeight * kLegendTypeLineCount) +
      (separatorGapPx * std::max(0, kLegendTypeLineCount - 1));
  const double rowHeight = totalRows > 0 ? availableHeight / totalRows : 0.0;
  const int desiredRowHeightPx =
      std::max(lineHeight,
               static_cast<int>(std::lround(rowHeight * renderZoom *
                                            kLegendLineSpacingScale)));
  const int rowHeightPx = std::max(1, std::min(desiredRowHeightPx, maxRowHeightPx));
  const double currentFontScale =
      std::clamp(static_cast<double>(fontSizePx) /
                     (14.0 * kLegendFontScale * renderZoom),
                 0.0, 1.0);

  int wrapFontPx =
      std::max(1, static_cast<int>(std::lround(fontSize)));
  const int wrapRowLimitPx = std::max(
      1, totalRows > 0
             ? static_cast<int>(std::floor(availableHeight / totalRows))
             : static_cast<int>(std::floor(availableHeight)));
  wxFont wrapFont =
      layoutviewerpanel::detail::MakeSharedFont(wrapFontPx,
                                                wxFONTWEIGHT_NORMAL);
  std::unordered_map<wxString, wxSize, wxStringHash, wxStringEqual>
      wrapTextExtentCache;
  auto measureWrapTextWidth = [&](const wxString &text) {
    const wxFont previousFont = dc.GetFont();
    dc.SetFont(wrapFont);
    const int width = measureTextExtent(text, wrapTextExtentCache).GetWidth();
    dc.SetFont(previousFont);
    return width;
  };
  for (;;) {
    const wxFont previousFont = dc.GetFont();
    dc.SetFont(wrapFont);
    int wrapTextHeight = 0;
    int ignoredWidth = 0;
    dc.GetTextExtent("Hg", &ignoredWidth, &wrapTextHeight);
    dc.SetFont(previousFont);
    if (wrapTextHeight <= wrapRowLimitPx || wrapFontPx <= 1)
      break;
    --wrapFontPx;
    wrapFont = layoutviewerpanel::detail::MakeSharedFont(wrapFontPx,
                                                          wxFONTWEIGHT_NORMAL);
    wrapTextExtentCache.clear();
  }

  baseTextExtentCache.clear();
  dc.SetFont(baseFont);
  int maxCountWidth = measureTextWidth("Count");
  int maxChWidth = showChannelColumn ? measureTextWidth("Ch") : 0;
  for (const auto &row : rowTexts) {
    maxCountWidth = std::max(maxCountWidth, measureTextWidth(row.countText));
    if (showChannelColumn)
      maxChWidth = std::max(maxChWidth, measureTextWidth(row.chText));
  }
  if (showChannelColumn) {
    const int chExtraWidthPx = measureTextWidth("0");
    maxChWidth += chExtraWidthPx;
  }
  const int desiredSymbolSize = static_cast<int>(std::lround(
      kLegendSymbolSizePx * renderZoom * currentFontScale));
  const int symbolSize = std::max(4, desiredSymbolSize);
  const double fallbackSymbolSize =
      std::max(4.0, static_cast<double>(symbolSize) * kLegendFallbackSymbolScale);
  const double svgSymbolSize =
      std::max(4.0, static_cast<double>(symbolSize) * kLegendSvgSymbolScale);
  auto symbolDrawWidth = [&](const SymbolDefinition *symbol) -> double {
    if (!symbol)
      return 0.0;
    const float symbolW = symbol->bounds.max.x - symbol->bounds.min.x;
    const float symbolH = symbol->bounds.max.y - symbol->bounds.min.y;
    if (symbolW <= 0.0f || symbolH <= 0.0f)
      return 0.0;
    double scale =
        std::min(fallbackSymbolSize / symbolW, fallbackSymbolSize / symbolH);
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
        std::min(fallbackSymbolSize / symbolW, fallbackSymbolSize / symbolH);
    return symbolH * scale;
  };

  std::vector<symbol_cache::FixtureSymbolSvgCache::SymbolHandle> svgHandles;
  // Resolves immutable SVG symbols through the managed runtime cache.
  auto findSvgSymbol = [&](const std::string &symbolKey,
                           SymbolViewKind view)
      -> const PerastageSvgSymbolData * {
    auto handle = symbol_cache::LoadUsableFixtureSymbol(symbolKey, view);
    if (!handle)
      return nullptr;
    svgHandles.push_back(std::move(handle));
    return svgHandles.back().get();
  };
  struct SvgGeometryMetrics {
    bool valid = false;
    double minX = 0.0;
    double minY = 0.0;
    double width = 0.0;
    double height = 0.0;
  };
  auto svgGeometryMetrics =
      [&](const PerastageSvgSymbolData *symbol) -> SvgGeometryMetrics {
    SvgGeometryMetrics metrics;
    if (!symbol)
      return metrics;

    bool hasPoint = false;
    double minX = 0.0;
    double minY = 0.0;
    double maxX = 0.0;
    double maxY = 0.0;
    auto includePoint = [&](const PerastageSvgPoint &pt) {
      if (!hasPoint) {
        minX = maxX = pt.x;
        minY = maxY = pt.y;
        hasPoint = true;
        return;
      }
      minX = std::min(minX, pt.x);
      minY = std::min(minY, pt.y);
      maxX = std::max(maxX, pt.x);
      maxY = std::max(maxY, pt.y);
    };

    for (const auto &polygon : symbol->fills) {
      for (const auto &pt : polygon.points)
        includePoint({pt.x + symbol->offsetXmm, pt.y + symbol->offsetYmm});
      for (const auto &hole : polygon.holes)
        for (const auto &pt : hole)
          includePoint({pt.x + symbol->offsetXmm, pt.y + symbol->offsetYmm});
    }
    for (const auto &line : symbol->strokes)
      for (const auto &pt : line.points)
        includePoint({pt.x + symbol->offsetXmm, pt.y + symbol->offsetYmm});

    if (!hasPoint)
      return metrics;

    metrics.minX = minX;
    metrics.minY = minY;
    metrics.width = std::max(0.0, maxX - minX);
    metrics.height = std::max(0.0, maxY - minY);
    metrics.valid = metrics.width > 0.0 && metrics.height > 0.0;
    return metrics;
  };
  auto svgScaleForDraw = [&](const PerastageSvgSymbolData *symbol) -> double {
    SvgGeometryMetrics metrics = svgGeometryMetrics(symbol);
    if (!metrics.valid)
      return 0.0;
    return std::min(svgSymbolSize / metrics.width, svgSymbolSize / metrics.height);
  };
  auto symbolDrawWidthSvg = [&](const PerastageSvgSymbolData *symbol) -> double {
    SvgGeometryMetrics metrics = svgGeometryMetrics(symbol);
    const double scale = svgScaleForDraw(symbol);
    if (!metrics.valid || scale <= 0.0)
      return 0.0;
    return metrics.width * scale;
  };
  auto symbolDrawHeightSvg = [&](const PerastageSvgSymbolData *symbol) -> double {
    SvgGeometryMetrics metrics = svgGeometryMetrics(symbol);
    const double scale = svgScaleForDraw(symbol);
    if (!metrics.valid || scale <= 0.0)
      return 0.0;
    return metrics.height * scale;
  };

  double maxTopSymbolColumnWidth = 0.0;
  double maxFrontSymbolColumnWidth = 0.0;
  double maxSideSymbolColumnWidth = 0.0;
  bool hasSvgSymbols = false;
  bool hasFallbackSymbols = false;
  bool hasTopSvgSymbols = false;
  bool hasFrontSvgSymbols = false;
  bool hasSideSvgSymbols = false;
  for (const auto &item : items) {
    if (item.symbolKey.empty())
      continue;
    const PerastageSvgSymbolData *topSvg = item.showBottomSymbol
                                               ? findSvgSymbol(item.symbolKey, SymbolViewKind::Bottom)
                                               : nullptr;
    const PerastageSvgSymbolData *frontSvg = item.showFrontSymbol
                                                 ? findSvgSymbol(item.symbolKey, SymbolViewKind::Front)
                                                 : nullptr;
    const PerastageSvgSymbolData *sideSvg = item.showSideSymbol
                                                ? findSvgSymbol(item.symbolKey, SymbolViewKind::Right)
                                                : nullptr;
    const SymbolDefinition *topSymbol = item.showBottomSymbol
                                            ? FindSymbolDefinitionPreferred(symbols, item.symbolKey,
                                                                           SymbolViewKind::Bottom)
                                            : nullptr;
    const SymbolDefinition *frontSymbol = item.showFrontSymbol
                                              ? FindSymbolDefinitionExact(symbols, item.symbolKey,
                                                                          SymbolViewKind::Front)
                                              : nullptr;
    const SymbolDefinition *sideSymbol = item.showSideSymbol
                                             ? FindSymbolDefinitionExact(symbols, item.symbolKey,
                                                                         SymbolViewKind::Right)
                                             : nullptr;
    if (topSvg) {
      hasSvgSymbols = true;
      hasTopSvgSymbols = true;
    } else if (topSymbol) {
      hasFallbackSymbols = true;
    }
    if (frontSvg) {
      hasSvgSymbols = true;
      hasFrontSvgSymbols = true;
    } else if (frontSymbol) {
      hasFallbackSymbols = true;
    }
    if (sideSvg) {
      hasSvgSymbols = true;
      hasSideSvgSymbols = true;
    } else if (sideSymbol) {
      hasFallbackSymbols = true;
    }
    const double topDrawW =
        topSvg ? symbolDrawWidthSvg(topSvg) : symbolDrawWidth(topSymbol);
    const double frontDrawW =
        frontSvg ? symbolDrawWidthSvg(frontSvg) : symbolDrawWidth(frontSymbol);
    const double sideDrawW =
        sideSvg ? symbolDrawWidthSvg(sideSvg) : symbolDrawWidth(sideSymbol);
    maxTopSymbolColumnWidth = std::max(maxTopSymbolColumnWidth, topDrawW);
    maxFrontSymbolColumnWidth = std::max(maxFrontSymbolColumnWidth, frontDrawW);
    maxSideSymbolColumnWidth = std::max(maxSideSymbolColumnWidth, sideDrawW);
  }
  const int maxSymbolColumnSize =
      std::max(4, static_cast<int>(std::lround(symbolSize *
                                               kLegendMaxSymbolSlotScale)));
  const int limitedSymbolColumnSize = std::max(4, (maxSymbolColumnSize * 2) / 5);
  const double maxMeasuredSymbolColumnWidth =
      static_cast<double>(limitedSymbolColumnSize);
  int topSymbolColumnSize = std::clamp(
      static_cast<int>(std::ceil(std::min(maxTopSymbolColumnWidth,
                                          maxMeasuredSymbolColumnWidth) *
                                 kLegendSymbolColumnScale)),
      0, limitedSymbolColumnSize);
  int frontSymbolColumnSize = std::clamp(
      static_cast<int>(std::ceil(std::min(maxFrontSymbolColumnWidth,
                                          maxMeasuredSymbolColumnWidth) *
                                 kLegendSymbolColumnScale)),
      0, limitedSymbolColumnSize);
  int sideSymbolColumnSize = std::clamp(
      static_cast<int>(std::ceil(std::min(maxSideSymbolColumnWidth,
                                          maxMeasuredSymbolColumnWidth) *
                                 kLegendSymbolColumnScale)),
      0, limitedSymbolColumnSize);
  const int columnGapPx =
      std::max(0, static_cast<int>(std::lround(columnGap * renderZoom)));
  int symbolColumnGapPx =
      std::max(0, static_cast<int>(std::lround(symbolColumnGap * renderZoom)));
  int symbolOuterMarginPx = 0;

  if (hasSvgSymbols && !hasFallbackSymbols) {
    const int minSvgColumnSize =
        std::max(4, static_cast<int>(std::lround(symbolSize * 0.55)));
    if (hasTopSvgSymbols)
      topSymbolColumnSize = std::max(topSymbolColumnSize, minSvgColumnSize);
    if (hasFrontSvgSymbols)
      frontSymbolColumnSize =
          std::max(frontSymbolColumnSize, minSvgColumnSize);
    if (hasSideSvgSymbols)
      sideSymbolColumnSize = std::max(sideSymbolColumnSize, minSvgColumnSize);
    symbolColumnGapPx =
        std::max(symbolColumnGapPx,
                 std::max(1, static_cast<int>(std::lround(2.5 * renderZoom))));
    symbolOuterMarginPx =
        std::max(1, static_cast<int>(std::lround(2.0 * renderZoom)));
  }

  int xTopSymbol = paddingLeftPx + symbolOuterMarginPx;
  int xFrontSymbol = xTopSymbol + topSymbolColumnSize + symbolColumnGapPx;
  int xSideSymbol = xFrontSymbol + frontSymbolColumnSize + symbolColumnGapPx;
  int xCount = xSideSymbol + sideSymbolColumnSize + columnGapPx;
  int xType = xCount + maxCountWidth + columnGapPx;
  int xCh = size.GetWidth() - paddingRightPx - maxChWidth;
  if (showChannelColumn) {
    if (xCh < xType + columnGapPx)
      xCh = xType + columnGapPx;
  } else {
    xCh = size.GetWidth() - paddingRightPx;
  }
  int typeWidth = std::max(
      0, showChannelColumn ? (xCh - xType - columnGapPx) : (xCh - xType));
  const int logicalSizeWidth =
      logicalSize.GetWidth() > 0 ? logicalSize.GetWidth() : size.GetWidth();
  int maxCountWidthLogical = measureWrapTextWidth("Count");
  int maxChWidthLogical = showChannelColumn ? measureWrapTextWidth("Ch") : 0;
  for (const auto &row : rowTexts) {
    maxCountWidthLogical =
        std::max(maxCountWidthLogical, measureWrapTextWidth(row.countText));
    if (showChannelColumn) {
      maxChWidthLogical =
          std::max(maxChWidthLogical, measureWrapTextWidth(row.chText));
    }
  }
  if (showChannelColumn) {
    maxChWidthLogical += measureWrapTextWidth("0");
  }
  const int topSymbolColumnSizeLogical = std::max(
      0, static_cast<int>(std::lround(static_cast<double>(topSymbolColumnSize) /
                                      renderZoom)));
  const int frontSymbolColumnSizeLogical =
      std::max(0, static_cast<int>(std::lround(
                      static_cast<double>(frontSymbolColumnSize) / renderZoom)));
  const int sideSymbolColumnSizeLogical = std::max(
      0, static_cast<int>(std::lround(static_cast<double>(sideSymbolColumnSize) /
                                      renderZoom)));
  const int symbolColumnGapLogical = std::max(
      0, static_cast<int>(std::lround(static_cast<double>(symbolColumnGapPx) /
                                      renderZoom)));
  const int symbolOuterMarginLogical = std::max(
      0, static_cast<int>(std::lround(static_cast<double>(symbolOuterMarginPx) /
                                      renderZoom)));
  const int xTopSymbolLogical = paddingLeft + symbolOuterMarginLogical;
  const int xFrontSymbolLogical =
      xTopSymbolLogical + topSymbolColumnSizeLogical + symbolColumnGapLogical;
  const int xSideSymbolLogical =
      xFrontSymbolLogical + frontSymbolColumnSizeLogical + symbolColumnGapLogical;
  const int xCountLogical =
      xSideSymbolLogical + sideSymbolColumnSizeLogical + columnGap;
  const int xTypeLogical = xCountLogical + maxCountWidthLogical + columnGap;
  int xChLogical = logicalSizeWidth - paddingRight - maxChWidthLogical;
  if (showChannelColumn) {
    if (xChLogical < xTypeLogical + columnGap)
      xChLogical = xTypeLogical + columnGap;
  } else {
    xChLogical = logicalSizeWidth - paddingRight;
  }
  const int typeWidthLogical = std::max(
      0, showChannelColumn ? (xChLogical - xTypeLogical - columnGap)
                           : (xChLogical - xTypeLogical));

  auto wrapTextToTwoLines = [&](const wxString &text, int maxWidth) {
    const int wrapMaxWidth =
        typeWidthLogical > 0
            ? typeWidthLogical
            : std::max(0, static_cast<int>(std::lround(maxWidth / renderZoom)));
    if (wrapMaxWidth <= 0)
      return std::array<wxString, 2>{wxString(), wxString()};

    if (measureWrapTextWidth(text) <= wrapMaxWidth)
      return std::array<wxString, 2>{text, wxString()};

    wxString firstLine = text;
    int splitAt = -1;
    for (int i = static_cast<int>(text.length()) - 1; i > 0; --i) {
      if (text[static_cast<size_t>(i)] == ' ') {
        wxString candidate = text.Left(static_cast<size_t>(i));
        if (!candidate.empty() &&
            measureWrapTextWidth(candidate) <= wrapMaxWidth) {
          splitAt = i;
          break;
        }
      }
    }

    if (splitAt < 0) {
      firstLine.clear();
      for (size_t i = 0; i < text.length(); ++i) {
        wxString candidate = text.Left(i + 1);
        if (measureWrapTextWidth(candidate) > wrapMaxWidth)
          break;
        firstLine = candidate;
      }
      if (firstLine.empty() && !text.empty())
        firstLine = text.Left(1);
    } else {
      firstLine = text.Left(static_cast<size_t>(splitAt));
    }

    wxString secondLine = text.Mid(firstLine.length());
    secondLine.Trim(true);
    secondLine.Trim(false);
    return std::array<wxString, 2>{firstLine, secondLine};
  };

  int y = paddingTopPx;
  const int headerTextOffset = std::max(0, (rowHeightPx - textHeight) / 2);
  const int rowSingleTextOffset = std::max(0, (rowHeightPx - textHeight) / 2);
  const int typeBlockHeight =
      (textHeight * kLegendTypeLineCount) + separatorGapPx;
  const int rowTypeTextOffset = std::max(0, (rowHeightPx - typeBlockHeight) / 2);
  dc.SetFont(headerFont);
  dc.DrawText("Count", xCount, y + headerTextOffset);
  dc.DrawText("Type", xType, y + headerTextOffset);
  if (showChannelColumn)
    dc.DrawText("Ch", xCh, y + headerTextOffset);

  y += rowHeightPx;
  dc.SetPen(wxPen(wxColour(200, 200, 200)));
  dc.DrawLine(xTopSymbol, y, size.GetWidth() - paddingRightPx, y);
  y += separatorGapPx;

  dc.SetFont(baseFont);
  LegendSymbolBackend backend(dc);
  for (size_t index = 0; index < items.size(); ++index) {
    const auto &item = items[index];
    const auto &rowText = rowTexts[index];
    if (y + rowHeightPx > size.GetHeight() - paddingBottomPx)
      break;
    const auto wrappedType = wrapTextToTwoLines(rowText.typeText, typeWidth);
    if (!item.symbolKey.empty()) {
      const SymbolDefinition *topSymbol = FindSymbolDefinitionPreferred(
          symbols, item.symbolKey, SymbolViewKind::Bottom);
      const SymbolDefinition *frontSymbol = FindSymbolDefinitionExact(
          symbols, item.symbolKey, SymbolViewKind::Front);
      const SymbolDefinition *sideSymbol = FindSymbolDefinitionExact(
          symbols, item.symbolKey, SymbolViewKind::Right);
      const PerastageSvgSymbolData *topSvg =
          findSvgSymbol(item.symbolKey, SymbolViewKind::Bottom);
      const PerastageSvgSymbolData *frontSvg =
          findSvgSymbol(item.symbolKey, SymbolViewKind::Front);
      const PerastageSvgSymbolData *sideSvg =
          findSvgSymbol(item.symbolKey, SymbolViewKind::Right);
      auto drawSymbol = [&](const SymbolDefinition *symbol, double drawCenterX,
                            double drawTop) {
        if (!symbol)
          return;
        const float symbolW = symbol->bounds.max.x - symbol->bounds.min.x;
        const float symbolH = symbol->bounds.max.y - symbol->bounds.min.y;
        if (symbolW <= 0.0f || symbolH <= 0.0f)
          return;
        double scale =
            std::min(fallbackSymbolSize / symbolW, fallbackSymbolSize / symbolH);
        double drawH = symbolH * scale;
        viewer2d::Viewer2DRenderMapping mapping{};
        mapping.minX = symbol->bounds.min.x;
        mapping.minY = symbol->bounds.min.y;
        mapping.scale = scale;
        mapping.offsetX =
            drawCenterX - (0.0 - static_cast<double>(mapping.minX)) * mapping.scale;
        mapping.offsetY = drawTop;
        mapping.drawHeight = drawH;
        backend.SetStrokeScale(
            mapping.scale > 0.0 ? 1.0 / mapping.scale : 1.0);
        RenderLegendCommandBuffer(symbol->localCommands,
                                  Transform2D::Identity(), symbols, backend,
                                  mapping);
      };
      auto drawSvg = [&](const PerastageSvgSymbolData *symbol,
                         const std::optional<std::string> &fillHex,
                         double drawLeft, double drawTop) {
        if (!symbol)
          return;
        const SvgGeometryMetrics metrics = svgGeometryMetrics(symbol);
        const double scale = svgScaleForDraw(symbol);
        if (!metrics.valid || scale <= 0.0)
          return;
        wxGraphicsContext *gc = dc.GetGraphicsContext();
        if (!gc)
          return;
        gc->PushState();
        gc->Translate(drawLeft, drawTop);
        gc->Scale(scale, scale);
        gc->Translate(symbol->offsetXmm, symbol->offsetYmm);
        gc->Translate(-metrics.minX, -metrics.minY);
        gc->SetBrush(wxBrush(ResolveLegendSvgFillColor(fillHex)));
        gc->SetPen(*wxTRANSPARENT_PEN);
        for (const auto &polygon : symbol->fills) {
          if (polygon.points.empty())
            continue;
          wxGraphicsPath path = gc->CreatePath();
          path.MoveToPoint(polygon.points.front().x, polygon.points.front().y);
          for (size_t i = 1; i < polygon.points.size(); ++i)
            path.AddLineToPoint(polygon.points[i].x, polygon.points[i].y);
          path.CloseSubpath();
          for (const auto &hole : polygon.holes) {
            if (hole.size() < 3)
              continue;
            path.MoveToPoint(hole.front().x, hole.front().y);
            for (size_t i = 1; i < hole.size(); ++i)
              path.AddLineToPoint(hole[i].x, hole[i].y);
            path.CloseSubpath();
          }
          gc->FillPath(path, wxODDEVEN_RULE);
        }
        gc->SetBrush(*wxTRANSPARENT_BRUSH);
        gc->SetPen(wxPen(*wxBLACK, 1));
        for (const auto &line : symbol->strokes) {
          if (line.points.size() < 2)
            continue;
          wxGraphicsPath path = gc->CreatePath();
          path.MoveToPoint(line.points.front().x, line.points.front().y);
          for (size_t i = 1; i < line.points.size(); ++i)
            path.AddLineToPoint(line.points[i].x, line.points[i].y);
          gc->StrokePath(path);
        }
        gc->PopState();
      };
      const double topDrawW =
          topSvg ? symbolDrawWidthSvg(topSvg) : symbolDrawWidth(topSymbol);
      const double frontDrawW =
          frontSvg ? symbolDrawWidthSvg(frontSvg) : symbolDrawWidth(frontSymbol);
      const double topDrawH =
          topSvg ? symbolDrawHeightSvg(topSvg) : symbolDrawHeight(topSymbol);
      const double frontDrawH =
          frontSvg ? symbolDrawHeightSvg(frontSvg)
                   : symbolDrawHeight(frontSymbol);
      const double sideDrawW =
          sideSvg ? symbolDrawWidthSvg(sideSvg) : symbolDrawWidth(sideSymbol);
      const double sideDrawH =
          sideSvg ? symbolDrawHeightSvg(sideSvg) : symbolDrawHeight(sideSymbol);
      if (item.showBottomSymbol && topDrawW > 0.0) {
        const double symbolDrawTop =
            y + (static_cast<double>(rowHeightPx) - topDrawH) * 0.5;
        const double symbolDrawLeft =
            xTopSymbol +
            std::max(0.0, (static_cast<double>(topSymbolColumnSize) - topDrawW) *
                                 0.5);
        if (topSvg)
          drawSvg(topSvg, item.symbolFillHex, symbolDrawLeft, symbolDrawTop);
        else
          drawSymbol(topSymbol,
                     xTopSymbol + static_cast<double>(topSymbolColumnSize) * 0.5,
                     symbolDrawTop);
      }
      if (item.showFrontSymbol && frontDrawW > 0.0) {
        const double symbolDrawTop =
            y + (static_cast<double>(rowHeightPx) - frontDrawH) * 0.5;
        const double symbolDrawLeft =
            xFrontSymbol +
            std::max(0.0, (static_cast<double>(frontSymbolColumnSize) - frontDrawW) *
                                 0.5);
        if (frontSvg)
          drawSvg(frontSvg, item.symbolFillHex, symbolDrawLeft, symbolDrawTop);
        else
          drawSymbol(frontSymbol,
                     xFrontSymbol + static_cast<double>(frontSymbolColumnSize) * 0.5,
                     symbolDrawTop);
      }
      if (item.showSideSymbol && sideDrawW > 0.0) {
        const double symbolDrawTop =
            y + (static_cast<double>(rowHeightPx) - sideDrawH) * 0.5;
        const double symbolDrawLeft =
            xSideSymbol +
            std::max(0.0, (static_cast<double>(sideSymbolColumnSize) - sideDrawW) *
                                 0.5);
        if (sideSvg)
          drawSvg(sideSvg, item.symbolFillHex, symbolDrawLeft, symbolDrawTop);
        else
          drawSymbol(sideSymbol,
                     xSideSymbol + static_cast<double>(sideSymbolColumnSize) * 0.5,
                     symbolDrawTop);
      }
    }
    dc.DrawText(rowText.countText, xCount, y + rowSingleTextOffset);
    const bool hasSecondTypeLine = !wrappedType[1].empty();
    const int firstTypeOffset =
        hasSecondTypeLine ? rowTypeTextOffset : rowSingleTextOffset;
    dc.DrawText(wrappedType[0], xType, y + firstTypeOffset);
    if (hasSecondTypeLine)
      dc.DrawText(wrappedType[1], xType,
                  y + rowTypeTextOffset + textHeight + separatorGapPx);
    if (showChannelColumn)
      dc.DrawText(rowText.chText, xCh, y + rowSingleTextOffset);
    y += rowHeightPx;
  }

  memoryDc.SelectObject(wxNullBitmap);
  return bitmap.ConvertToImage();
}
