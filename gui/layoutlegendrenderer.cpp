#include "layoutlegendrenderer.h"
#include "paper_stroke_mapping.h"
#include "projectutils.h"
#include "logger.h"
#include "typography/font_resources.h"
#include "document_text_renderer.h"
#include "legend_composition_adapter.h"
#include "symbols/fixture_symbol_availability.h"
#include "viewer2dcommandrenderer.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <tuple>
#include <wx/dcgraph.h>
#include <wx/dcmemory.h>
#include <wx/graphics.h>

namespace {
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

void RenderLegendCommandBuffer(const CommandBuffer &buffer,
                               const Transform2D &localTransform,
                               const SymbolDefinitionSnapshot *symbols,
                               LegendSymbolBackend &backend,
                               const viewer2d::Viewer2DRenderMapping &mapping);

void RenderLegendDrawCommand(const CanvasCommand &command,
                             const Transform2D &localTransform,
                             const CanvasTransform &currentTransform,
                             const SymbolDefinitionSnapshot *symbols,
                             LegendSymbolBackend &backend,
                             const viewer2d::Viewer2DRenderMapping &mapping,
                             bool drawStrokes, bool drawFills);

wxColour ToWxColor(const CanvasColor &color) {
  auto clamp = [](float v) {
    return static_cast<unsigned char>(std::clamp(v, 0.0f, 1.0f) * 255.0f);
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

  void SetStrokeScale(double scale) { strokeScale_ = scale; }

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

  wxGraphicsPen GraphicsStroke(const CanvasStroke &stroke, double width) const {
    if (!drawStrokes_ || width * strokeScale_ <= 0)
      return gc_->CreatePen(*wxTRANSPARENT_PEN);
    return gc_->CreatePen(wxGraphicsPenInfo(ToWxColor(stroke.color))
        .Width(width * strokeScale_).Cap(wxCAP_ROUND).Join(wxJOIN_ROUND));
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
      gc_->SetPen(GraphicsStroke(stroke, strokeWidthPx));
      gc_->StrokeLine(p0.x, p0.y, p1.x, p1.y);
      return;
    }
    dc_.SetPen(pen);
    dc_.SetBrush(*wxTRANSPARENT_BRUSH);
    dc_.DrawLine(wxPoint(std::lround(p0.x), std::lround(p0.y)),
                 wxPoint(std::lround(p1.x), std::lround(p1.y)));
  }

  void DrawPolyline(const std::vector<viewer2d::Viewer2DRenderPoint> &points,
                    const CanvasStroke &stroke, double strokeWidthPx) override {
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
      gc_->SetPen(GraphicsStroke(stroke, strokeWidthPx));
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
    wxBrush brush = shouldFill ? MakeFillBrush(fill) : *wxTRANSPARENT_BRUSH;
    if (gc_) {
      wxGraphicsPath path = gc_->CreatePath();
      path.MoveToPoint(points.front().x, points.front().y);
      for (size_t i = 1; i < points.size(); ++i)
        path.AddLineToPoint(points[i].x, points[i].y);
      path.CloseSubpath();
      gc_->SetBrush(brush);
      gc_->SetPen(GraphicsStroke(stroke, strokeWidthPx));
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
    wxBrush brush = shouldFill ? MakeFillBrush(fill) : *wxTRANSPARENT_BRUSH;
    if (gc_) {
      if (shouldFill || shouldStroke) {
        gc_->SetBrush(brush);
        gc_->SetPen(GraphicsStroke(stroke, strokeWidthPx));
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

void RenderLegendDrawCommand(const CanvasCommand &command,
                             const Transform2D &localTransform,
                             const CanvasTransform &currentTransform,
                             const SymbolDefinitionSnapshot *symbols,
                             LegendSymbolBackend &backend,
                             const viewer2d::Viewer2DRenderMapping &mapping,
                             bool drawStrokes, bool drawFills) {
  auto strokeWidth = [&](float width) { return width * mapping.scale; };
  std::visit(
      [&](auto &&cmd) {
        using T = std::decay_t<decltype(cmd)>;
        if constexpr (std::is_same_v<T, LineCommand>) {
          if (!drawStrokes)
            return;
          viewer2d::Viewer2DRenderPoint p0 = MapLegendPoint(
              localTransform, currentTransform, mapping, cmd.x0, cmd.y0);
          viewer2d::Viewer2DRenderPoint p1 = MapLegendPoint(
              localTransform, currentTransform, mapping, cmd.x1, cmd.y1);
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
          std::vector<float> pts = {cmd.x, cmd.y,         cmd.x + cmd.w,
                                    cmd.y, cmd.x + cmd.w, cmd.y + cmd.h,
                                    cmd.x, cmd.y + cmd.h};
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
          viewer2d::Viewer2DRenderPoint center = MapLegendPoint(
              localTransform, currentTransform, mapping, cmd.cx, cmd.cy);
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
          Transform2D combined =
              ComposeLegendTransform(localTransform, cmd.transform);
          RenderLegendCommandBuffer(it->second.localCommands, combined, symbols,
                                    backend, mapping);
        } else {
          (void)cmd;
        }
      },
      command);
}

void RenderLegendCommandBuffer(const CommandBuffer &buffer,
                               const Transform2D &localTransform,
                               const SymbolDefinitionSnapshot *symbols,
                               LegendSymbolBackend &backend,
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

namespace gui {
using namespace layouts::composition;
wxImage RenderLegendImage(const wxSize &size, const wxSize &logicalSize,
    double renderZoom, const std::vector<layouts::LegendResolvedItem> &items,
    const layouts::LayoutLegendDefinition &legend,
    const SymbolDefinitionSnapshot *symbols, LegendCompositionPlan *composedPlan) {
  if (size.GetWidth() <= 0 || size.GetHeight() <= 0 || renderZoom <= 0)
    return {};
  wxBitmap bitmap(size.GetWidth(), size.GetHeight(), 32);
  wxMemoryDC memoryDc(bitmap);
  wxGCDC dc(memoryDc);
  std::vector<std::shared_ptr<const PerastageSvgSymbolData>> handles;
  auto resources = viewer2d::ResolveLegendComposition(items, symbols,
      [&](const std::string &key, const std::string &, SymbolViewKind view) {
        handles.push_back(symbol_cache::LoadUsableFixtureSymbol(key, view));
        return handles.back().get();
      });
  auto &input = resources.input;
  input.width = logicalSize.GetWidth() > 0 ? logicalSize.GetWidth() : size.GetWidth() / renderZoom;
  input.height = logicalSize.GetHeight() > 0 ? logicalSize.GetHeight() : size.GetHeight() / renderZoom;
  input.showChannelColumn = legend.showChannelColumn;
  static const typography::FontPair fonts(ProjectUtils::GetResourceRoot());
  const auto plan = ComposeLegend(input, {fonts.regular, fonts.bold});
  if (composedPlan) *composedPlan = plan;
  for (const auto &diagnostic : plan.diagnostics) Logger::Instance().Log(diagnostic);
  dc.SetBackground(wxBrush(*wxWHITE));
  dc.Clear();
  dc.SetTextForeground(wxColour(20, 20, 20));
  if (auto *gc = dc.GetGraphicsContext()) {
    gc->PushState(); gc->Scale(renderZoom, renderZoom);
    gc->SetPen(gc->CreatePen(wxGraphicsPenInfo(wxColour(200,200,200)).Width(1).Cap(wxCAP_BUTT).Join(wxJOIN_MITER)));
    gc->StrokeLine(plan.separatorStart.x, plan.separatorStart.y, plan.separatorEnd.x, plan.separatorEnd.y);
    gc->PopState();
  }
  LegendSymbolBackend backend(dc);
  for (const auto &placement : plan.symbols) {
    const auto &resolved = resources.symbols[placement.geometry.reference];
    const auto &bounds = placement.geometry.bounds;
    const double scale = placement.scale * renderZoom;
    auto *gc = dc.GetGraphicsContext();
    if (!gc) continue;
    gc->PushState();
    gc->Clip(placement.slot.x * renderZoom, placement.slot.y * renderZoom,
             placement.slot.width * renderZoom, placement.slot.height * renderZoom);
    if (resolved.svg) {
      const auto &svg = *resolved.svg;
      gc->Translate(placement.draw.x * renderZoom, placement.draw.y * renderZoom);
      gc->Scale(scale, scale);
      gc->Translate(svg.offsetXmm - bounds.x, svg.offsetYmm - bounds.y);
      gc->SetBrush(wxBrush(ResolveLegendSvgFillColor(items[placement.row].symbolFillHex)));
      gc->SetPen(*wxTRANSPARENT_PEN);
      for (const auto &polygon : svg.fills) {
        if (polygon.points.empty()) continue;
        auto path = gc->CreatePath();
        auto add = [&](const std::vector<PerastageSvgPoint> &points) {
          if (points.empty()) return;
          path.MoveToPoint(points[0].x, points[0].y);
          for (std::size_t i = 1; i < points.size(); ++i) path.AddLineToPoint(points[i].x, points[i].y);
          path.CloseSubpath();
        };
        add(polygon.points);
        for (const auto &hole : polygon.holes) add(hole);
        gc->FillPath(path, wxODDEVEN_RULE);
      }
      gc->SetBrush(*wxTRANSPARENT_BRUSH);
      gc->SetPen(gc->CreatePen(wxGraphicsPenInfo(*wxBLACK).Width(1 / placement.scale).Cap(wxCAP_BUTT).Join(wxJOIN_MITER)));
      for (const auto &line : svg.strokes) {
        if (line.points.size() < 2) continue;
        auto path = gc->CreatePath();
        path.MoveToPoint(line.points[0].x, line.points[0].y);
        for (std::size_t i = 1; i < line.points.size(); ++i) path.AddLineToPoint(line.points[i].x, line.points[i].y);
        gc->StrokePath(path);
      }
    } else if (resolved.commands) {
      viewer2d::Viewer2DRenderMapping mapping;
      mapping.minX = bounds.x;
      mapping.minY = bounds.y;
      mapping.scale = scale;
      mapping.offsetX = placement.commandOriginX * renderZoom + bounds.x * scale;
      mapping.offsetY = placement.draw.y * renderZoom;
      mapping.drawHeight = placement.draw.height * renderZoom;
      backend.SetStrokeScale(viewer2d::PaperStrokeScale(placement.scale, 1.0, placement.geometry.strokeScale));
      RenderLegendCommandBuffer(resolved.commands->localCommands,
          Transform2D::Identity(), symbols, backend, mapping);
    }
    gc->PopState();
  }
  if (auto *gc = dc.GetGraphicsContext()) {
    gc->PushState();
    gc->Clip(0, 0, plan.clip.width * renderZoom, plan.clip.height * renderZoom);
    gc->SetBrush(wxBrush(wxColour(20,20,20)));
    gc->SetPen(*wxTRANSPARENT_PEN);
    for (const auto &line : plan.text) {
      gc->PushState();
      gc->Clip(line.clip.x*renderZoom,line.clip.y*renderZoom,line.clip.width*renderZoom,line.clip.height*renderZoom);
      gui::DrawComposedDocumentText(*gc, line.glyphRun,
          line.role == TextRole::Bold ? fonts.bold : fonts.regular,
          line.baseline.x, line.baseline.y, renderZoom);
      gc->PopState();
    }
    gc->PopState();
    gc->PushState(); gc->Scale(renderZoom,renderZoom);
    gc->SetBrush(*wxTRANSPARENT_BRUSH);
    gc->SetPen(gc->CreatePen(wxGraphicsPenInfo(*wxBLACK).Width(plan.borderWidth)));
    gc->DrawRectangle(0,0,plan.clip.width,plan.clip.height);
    gc->PopState();
  }
  memoryDc.SelectObject(wxNullBitmap);
  return bitmap.ConvertToImage();
}
} // namespace gui
