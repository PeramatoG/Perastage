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
#include <wx/dcbuffer.h>
#include <wx/graphics.h>

#include "configmanager.h"
#include "layouts/LayoutManager.h"
#include "viewer2d/viewer2dstate.h"

namespace {
constexpr double kMinZoom = 0.1;
constexpr double kMaxZoom = 10.0;
constexpr double kZoomStep = 1.1;
constexpr int kFitMarginPx = 40;
constexpr int kHandleSizePx = 10;
constexpr int kHandleHalfPx = kHandleSizePx / 2;
constexpr int kHandleHoverPadPx = 6;
constexpr int kMinFrameSize = 24;
constexpr int kEditMenuId = wxID_HIGHEST + 490;
constexpr int kDeleteMenuId = wxID_HIGHEST + 491;
constexpr double kPixelsPerMeter = 25.0;

struct ThumbnailMapping {
  double minX = 0.0;
  double minY = 0.0;
  double scale = 1.0;
  double offsetX = 0.0;
  double offsetY = 0.0;
  double drawHeight = 0.0;
};

wxColour ToWxColor(const CanvasColor &color) {
  auto clamp = [](float v) {
    return static_cast<unsigned char>(
        std::clamp(v, 0.0f, 1.0f) * 255.0f);
  };
  return wxColour(clamp(color.r), clamp(color.g), clamp(color.b),
                  clamp(color.a));
}

SymbolPoint ApplyTransformPoint(const Transform2D &t, float x, float y) {
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

ThumbnailMapping BuildMapping(const Viewer2DViewState &viewState,
                              const wxSize &targetSize) {
  double ppm = kPixelsPerMeter * static_cast<double>(viewState.zoom);
  double halfW = static_cast<double>(viewState.viewportWidth) / ppm * 0.5;
  double halfH = static_cast<double>(viewState.viewportHeight) / ppm * 0.5;
  double offX = static_cast<double>(viewState.offsetPixelsX) / kPixelsPerMeter;
  double offY = static_cast<double>(viewState.offsetPixelsY) / kPixelsPerMeter;
  double minX = -halfW - offX;
  double maxX = halfW - offX;
  double minY = -halfH - offY;
  double maxY = halfH - offY;

  double width = maxX - minX;
  double height = maxY - minY;
  if (width <= 0.0 || height <= 0.0) {
    ThumbnailMapping empty;
    empty.scale = 0.0;
    return empty;
  }

  double targetW = static_cast<double>(targetSize.GetWidth());
  double targetH = static_cast<double>(targetSize.GetHeight());
  if (targetW <= 0.0 || targetH <= 0.0) {
    ThumbnailMapping empty;
    empty.scale = 0.0;
    return empty;
  }
  double scale = std::min(targetW / width, targetH / height);
  double drawW = width * scale;
  double drawH = height * scale;
  double offsetX = (targetW - drawW) * 0.5;
  double offsetY = (targetH - drawH) * 0.5;

  ThumbnailMapping mapping;
  mapping.minX = minX;
  mapping.minY = minY;
  mapping.scale = scale;
  mapping.offsetX = offsetX;
  mapping.offsetY = offsetY;
  mapping.drawHeight = drawH;
  return mapping;
}

wxPoint2DDouble MapPoint(float x, float y, const Transform2D &localTransform,
                         const CanvasTransform &canvasTransform,
                         const ThumbnailMapping &mapping,
                         const wxPoint &origin) {
  auto transformed = ApplyTransformPoint(localTransform, x, y);
  double tx = transformed.x * canvasTransform.scale + canvasTransform.offsetX;
  double ty = transformed.y * canvasTransform.scale + canvasTransform.offsetY;
  double mappedX =
      origin.x + mapping.offsetX + (tx - mapping.minX) * mapping.scale;
  double mappedY = origin.y + mapping.offsetY + mapping.drawHeight -
                   (ty - mapping.minY) * mapping.scale;
  return {mappedX, mappedY};
}

void DrawTextLines(wxGraphicsContext &gc, const wxString &text,
                   const wxPoint2DDouble &anchor, double lineHeight,
                   CanvasTextStyle::HorizontalAlign hAlign,
                   CanvasTextStyle::VerticalAlign vAlign) {
  wxArrayString lines = wxSplit(text, '\n');
  if (lines.empty())
    return;

  double maxWidth = 0.0;
  double ascent = 0.0;
  double descent = 0.0;
  double totalHeight = 0.0;
  for (const auto &line : lines) {
    double w = 0.0;
    double h = 0.0;
    double externalLeading = 0.0;
    gc.GetTextExtent(line, &w, &h, &descent, &externalLeading);
    ascent = h - descent;
    maxWidth = std::max(maxWidth, w);
    totalHeight += lineHeight;
  }
  if (!lines.empty())
    totalHeight -= lineHeight;

  double x = anchor.m_x;
  if (hAlign == CanvasTextStyle::HorizontalAlign::Center)
    x -= maxWidth * 0.5;
  else if (hAlign == CanvasTextStyle::HorizontalAlign::Right)
    x -= maxWidth;

  double y = anchor.m_y;
  switch (vAlign) {
  case CanvasTextStyle::VerticalAlign::Top:
    break;
  case CanvasTextStyle::VerticalAlign::Middle:
    y -= totalHeight * 0.5;
    break;
  case CanvasTextStyle::VerticalAlign::Bottom:
    y -= totalHeight;
    break;
  case CanvasTextStyle::VerticalAlign::Baseline:
  default:
    y -= ascent;
    break;
  }

  for (size_t i = 0; i < lines.size(); ++i) {
    gc.DrawText(lines[i], x, y + lineHeight * static_cast<double>(i));
  }
}

void RenderCommands(wxGraphicsContext &gc, const CommandBuffer &buffer,
                    const Viewer2DViewState &viewState, const wxPoint &origin,
                    const wxSize &size, const Transform2D &localTransform,
                    const SymbolDefinitionSnapshot *symbols) {
  if (buffer.commands.empty())
    return;

  if (viewState.viewportWidth <= 0 || viewState.viewportHeight <= 0)
    return;

  ThumbnailMapping mapping = BuildMapping(viewState, size);
  if (mapping.scale <= 0.0)
    return;

  CanvasTransform currentTransform{};
  std::vector<CanvasTransform> stack;

  auto strokeWidth = [&](float width) {
    return std::max(1, static_cast<int>(std::lround(width * mapping.scale)));
  };

  auto drawSymbolInstance = [&](uint32_t symbolId,
                                const Transform2D &transform) -> void {
    if (!symbols)
      return;
    auto it = symbols->find(symbolId);
    if (it == symbols->end())
      return;
    Transform2D combined = ComposeTransform(localTransform, transform);
    RenderCommands(gc, it->second.localCommands, viewState, origin, size,
                   combined, symbols);
  };

  for (const auto &cmd : buffer.commands) {
    if (const auto *line = std::get_if<LineCommand>(&cmd)) {
      wxPoint2DDouble p0 =
          MapPoint(line->x0, line->y0, localTransform, currentTransform,
                   mapping, origin);
      wxPoint2DDouble p1 =
          MapPoint(line->x1, line->y1, localTransform, currentTransform,
                   mapping, origin);
      wxPen pen(ToWxColor(line->stroke.color), strokeWidth(line->stroke.width));
      gc.SetPen(pen);
      gc.StrokeLine(p0.m_x, p0.m_y, p1.m_x, p1.m_y);
    } else if (const auto *polyline = std::get_if<PolylineCommand>(&cmd)) {
      if (polyline->points.size() < 4)
        continue;
      wxGraphicsPath path = gc.CreatePath();
      wxPoint2DDouble start =
          MapPoint(polyline->points[0], polyline->points[1], localTransform,
                   currentTransform, mapping, origin);
      path.MoveToPoint(start);
      for (size_t i = 2; i + 1 < polyline->points.size(); i += 2) {
        wxPoint2DDouble pt =
            MapPoint(polyline->points[i], polyline->points[i + 1],
                     localTransform, currentTransform, mapping, origin);
        path.AddLineToPoint(pt);
      }
      wxPen pen(ToWxColor(polyline->stroke.color),
                strokeWidth(polyline->stroke.width));
      gc.SetPen(pen);
      gc.StrokePath(path);
    } else if (const auto *poly = std::get_if<PolygonCommand>(&cmd)) {
      if (poly->points.size() < 6)
        continue;
      wxGraphicsPath path = gc.CreatePath();
      wxPoint2DDouble start =
          MapPoint(poly->points[0], poly->points[1], localTransform,
                   currentTransform, mapping, origin);
      path.MoveToPoint(start);
      for (size_t i = 2; i + 1 < poly->points.size(); i += 2) {
        wxPoint2DDouble pt = MapPoint(poly->points[i], poly->points[i + 1],
                                      localTransform, currentTransform, mapping,
                                      origin);
        path.AddLineToPoint(pt);
      }
      path.CloseSubpath();
      if (poly->hasFill) {
        gc.SetBrush(wxBrush(ToWxColor(poly->fill.color)));
        gc.FillPath(path);
      }
      wxPen pen(ToWxColor(poly->stroke.color),
                strokeWidth(poly->stroke.width));
      gc.SetPen(pen);
      gc.StrokePath(path);
    } else if (const auto *rect = std::get_if<RectangleCommand>(&cmd)) {
      std::vector<float> pts = {rect->x, rect->y, rect->x + rect->w, rect->y,
                                rect->x + rect->w, rect->y + rect->h, rect->x,
                                rect->y + rect->h};
      wxGraphicsPath path = gc.CreatePath();
      wxPoint2DDouble start = MapPoint(pts[0], pts[1], localTransform,
                                       currentTransform, mapping, origin);
      path.MoveToPoint(start);
      for (size_t i = 2; i + 1 < pts.size(); i += 2) {
        wxPoint2DDouble pt =
            MapPoint(pts[i], pts[i + 1], localTransform, currentTransform,
                     mapping, origin);
        path.AddLineToPoint(pt);
      }
      path.CloseSubpath();
      if (rect->hasFill) {
        gc.SetBrush(wxBrush(ToWxColor(rect->fill.color)));
        gc.FillPath(path);
      }
      wxPen pen(ToWxColor(rect->stroke.color),
                strokeWidth(rect->stroke.width));
      gc.SetPen(pen);
      gc.StrokePath(path);
    } else if (const auto *circle = std::get_if<CircleCommand>(&cmd)) {
      auto center =
          MapPoint(circle->cx, circle->cy, localTransform, currentTransform,
                   mapping, origin);
      float sx =
          std::sqrt(localTransform.a * localTransform.a +
                    localTransform.b * localTransform.b);
      float sy =
          std::sqrt(localTransform.c * localTransform.c +
                    localTransform.d * localTransform.d);
      float scale = (sx + sy) * 0.5f;
      double radius = circle->radius * scale * currentTransform.scale *
                      mapping.scale;
      wxRect2DDouble rect(center.m_x - radius, center.m_y - radius,
                          radius * 2.0, radius * 2.0);
      if (circle->hasFill) {
        gc.SetBrush(wxBrush(ToWxColor(circle->fill.color)));
        gc.DrawEllipse(rect.m_x, rect.m_y, rect.m_width, rect.m_height);
      }
      wxPen pen(ToWxColor(circle->stroke.color),
                strokeWidth(circle->stroke.width));
      gc.SetPen(pen);
      gc.StrokeEllipse(rect.m_x, rect.m_y, rect.m_width, rect.m_height);
    } else if (const auto *text = std::get_if<TextCommand>(&cmd)) {
      wxPoint2DDouble anchor =
          MapPoint(text->x, text->y, localTransform, currentTransform, mapping,
                   origin);
      double fontSize = text->style.fontSize * mapping.scale;
      int pixelSize = std::max(1, static_cast<int>(std::lround(fontSize)));
      wxFontInfo fontInfo(pixelSize);
      if (!text->style.fontFamily.empty())
        fontInfo.FaceName(wxString::FromUTF8(text->style.fontFamily));
      wxFont font(fontInfo);
      gc.SetFont(font, ToWxColor(text->style.color));

      double lineHeight = fontSize;
      if (text->style.lineHeight > 0.0f)
        lineHeight = text->style.lineHeight * mapping.scale;
      if (lineHeight <= 0.0)
        lineHeight = fontSize;

      if (text->style.outlineWidth > 0.0f) {
        double outline =
            text->style.outlineWidth * mapping.scale;
        gc.SetFont(font, ToWxColor(text->style.outlineColor));
        const std::array<wxPoint2DDouble, 4> offsets = {
            wxPoint2DDouble{-outline, 0.0},
            wxPoint2DDouble{outline, 0.0},
            wxPoint2DDouble{0.0, -outline},
            wxPoint2DDouble{0.0, outline}};
        for (const auto &offset : offsets) {
          DrawTextLines(gc, wxString::FromUTF8(text->text),
                        wxPoint2DDouble(anchor.m_x + offset.m_x,
                                        anchor.m_y + offset.m_y),
                        lineHeight, text->style.hAlign, text->style.vAlign);
        }
        gc.SetFont(font, ToWxColor(text->style.color));
      }

      DrawTextLines(gc, wxString::FromUTF8(text->text), anchor, lineHeight,
                    text->style.hAlign, text->style.vAlign);
    } else if (const auto *save = std::get_if<SaveCommand>(&cmd)) {
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
    } else if (const auto *instance =
                   std::get_if<SymbolInstanceCommand>(&cmd)) {
      drawSymbolInstance(instance->symbolId, instance->transform);
    }
  }
}
}

wxDEFINE_EVENT(EVT_LAYOUT_VIEW_EDIT, wxCommandEvent);

wxBEGIN_EVENT_TABLE(LayoutViewerPanel, wxPanel)
    EVT_PAINT(LayoutViewerPanel::OnPaint)
    EVT_SIZE(LayoutViewerPanel::OnSize)
    EVT_LEFT_DOWN(LayoutViewerPanel::OnLeftDown)
    EVT_LEFT_UP(LayoutViewerPanel::OnLeftUp)
    EVT_LEFT_DCLICK(LayoutViewerPanel::OnLeftDClick)
    EVT_MOTION(LayoutViewerPanel::OnMouseMove)
    EVT_MOUSEWHEEL(LayoutViewerPanel::OnMouseWheel)
    EVT_MOUSE_CAPTURE_LOST(LayoutViewerPanel::OnCaptureLost)
    EVT_RIGHT_UP(LayoutViewerPanel::OnRightUp)
    EVT_MENU(kEditMenuId, LayoutViewerPanel::OnEditView)
    EVT_MENU(kDeleteMenuId, LayoutViewerPanel::OnDeleteView)
wxEND_EVENT_TABLE()

LayoutViewerPanel::LayoutViewerPanel(wxWindow *parent)
    : wxPanel(parent, wxID_ANY) {
  SetBackgroundStyle(wxBG_STYLE_PAINT);
  currentLayout.pageSetup.pageSize = print::PageSize::A4;
  currentLayout.pageSetup.landscape = false;
  ResetViewToFit();
}

void LayoutViewerPanel::SetLayoutDefinition(
    const layouts::LayoutDefinition &layout) {
  currentLayout = layout;
  layoutVersion++;
  captureVersion = -1;
  hasCapture = false;
  ResetViewToFit();
  Refresh();
}

void LayoutViewerPanel::OnPaint(wxPaintEvent &) {
  wxAutoBufferedPaintDC dc(this);
  dc.Clear();

  wxSize size = GetClientSize();
  dc.SetBrush(wxBrush(wxColour(90, 90, 90)));
  dc.SetPen(*wxTRANSPARENT_PEN);
  dc.DrawRectangle(0, 0, size.GetWidth(), size.GetHeight());

  const double pageWidth = currentLayout.pageSetup.PageWidthPt();
  const double pageHeight = currentLayout.pageSetup.PageHeightPt();

  const double scaledWidth = pageWidth * zoom;
  const double scaledHeight = pageHeight * zoom;

  const wxPoint center(size.GetWidth() / 2, size.GetHeight() / 2);
  const wxPoint topLeft(center.x - static_cast<int>(scaledWidth / 2.0) +
                            panOffset.x,
                        center.y - static_cast<int>(scaledHeight / 2.0) +
                            panOffset.y);

  dc.SetBrush(wxBrush(wxColour(255, 255, 255)));
  dc.SetPen(wxPen(wxColour(200, 200, 200)));
  dc.DrawRectangle(topLeft.x, topLeft.y, static_cast<int>(scaledWidth),
                   static_cast<int>(scaledHeight));

  const layouts::Layout2DViewDefinition *view = GetEditableView();
  if (!view)
    return;

  wxRect frameRect;
  if (!GetFrameRect(view->frame, frameRect))
    return;

  if (!captureInProgress && captureVersion != layoutVersion) {
    Viewer2DPanel *panel = Viewer2DPanel::Instance();
    if (panel) {
      captureInProgress = true;
      ConfigManager &cfg = ConfigManager::Get();
      viewer2d::Viewer2DState previousState =
          viewer2d::CaptureState(panel, cfg);
      viewer2d::Viewer2DState layoutState =
          viewer2d::FromLayoutDefinition(*view);
      viewer2d::ApplyState(panel, nullptr, cfg, layoutState);
      panel->CaptureFrameAsync(
          [this, previousState](CommandBuffer buffer, Viewer2DViewState state) {
            cachedBuffer = std::move(buffer);
            cachedViewState = state;
            cachedSymbols.reset();
            if (Viewer2DPanel::Instance()) {
              cachedSymbols =
                  Viewer2DPanel::Instance()->GetBottomSymbolCacheSnapshot();
            }
            hasCapture = !cachedBuffer.commands.empty();
            captureVersion = layoutVersion;
            captureInProgress = false;
            viewer2d::ApplyState(Viewer2DPanel::Instance(), nullptr,
                                 ConfigManager::Get(), previousState);
            Refresh();
          });
    }
  }

  if (hasCapture && frameRect.GetWidth() > 0 && frameRect.GetHeight() > 0) {
    wxBitmap bufferBitmap(frameRect.GetWidth(), frameRect.GetHeight());
    wxMemoryDC memDC(bufferBitmap);
    memDC.SetBackground(wxBrush(wxColour(255, 255, 255)));
    memDC.Clear();
    if (auto gc = wxGraphicsContext::Create(memDC)) {
      RenderCommands(*gc, cachedBuffer, cachedViewState, wxPoint(0, 0),
                     frameRect.GetSize(), Transform2D::Identity(),
                     cachedSymbols.get());
      delete gc;
    }
    memDC.SelectObject(wxNullBitmap);
    dc.DrawBitmap(bufferBitmap, frameRect.GetTopLeft(), false);
  }

  dc.SetBrush(*wxTRANSPARENT_BRUSH);
  dc.SetPen(wxPen(wxColour(60, 160, 240), 2));
  dc.DrawRectangle(frameRect);

  wxRect handleRight(frameRect.GetRight() - kHandleHalfPx,
                     frameRect.GetTop() + frameRect.GetHeight() / 2 -
                         kHandleHalfPx,
                     kHandleSizePx, kHandleSizePx);
  wxRect handleBottom(frameRect.GetLeft() + frameRect.GetWidth() / 2 -
                          kHandleHalfPx,
                      frameRect.GetBottom() - kHandleHalfPx, kHandleSizePx,
                      kHandleSizePx);
  wxRect handleCorner(frameRect.GetRight() - kHandleHalfPx,
                      frameRect.GetBottom() - kHandleHalfPx, kHandleSizePx,
                      kHandleSizePx);

  dc.SetBrush(wxBrush(wxColour(60, 160, 240)));
  dc.SetPen(*wxTRANSPARENT_PEN);
  dc.DrawRectangle(handleRight);
  dc.DrawRectangle(handleBottom);
  dc.DrawRectangle(handleCorner);
}

void LayoutViewerPanel::OnSize(wxSizeEvent &) {
  Refresh();
}

void LayoutViewerPanel::OnLeftDown(wxMouseEvent &event) {
  const wxPoint pos = event.GetPosition();
  const layouts::Layout2DViewDefinition *view = GetEditableView();
  wxRect frameRect;
  if (view && GetFrameRect(view->frame, frameRect)) {
    FrameDragMode mode = HitTestFrame(pos, frameRect);
    if (mode != FrameDragMode::None) {
      dragMode = mode;
      dragStartPos = pos;
      dragStartFrame = view->frame;
      CaptureMouse();
      return;
    }
  }

  isPanning = true;
  lastMousePos = pos;
  CaptureMouse();
}

void LayoutViewerPanel::OnLeftUp(wxMouseEvent &) {
  if (dragMode != FrameDragMode::None) {
    dragMode = FrameDragMode::None;
    if (HasCapture())
      ReleaseMouse();
    return;
  }
  if (isPanning) {
    isPanning = false;
    if (HasCapture())
      ReleaseMouse();
  }
}

void LayoutViewerPanel::OnLeftDClick(wxMouseEvent &event) {
  const wxPoint pos = event.GetPosition();
  const layouts::Layout2DViewDefinition *view = GetEditableView();
  wxRect frameRect;
  if (view && GetFrameRect(view->frame, frameRect) && frameRect.Contains(pos)) {
    EmitEditViewRequest();
    return;
  }
  event.Skip();
}

void LayoutViewerPanel::OnMouseMove(wxMouseEvent &event) {
  wxPoint currentPos = event.GetPosition();
  const layouts::Layout2DViewDefinition *view = GetEditableView();
  wxRect frameRect;
  if (view && GetFrameRect(view->frame, frameRect)) {
    hoverMode = HitTestFrame(currentPos, frameRect);
    SetCursor(CursorForMode(hoverMode));
  } else {
    hoverMode = FrameDragMode::None;
    SetCursor(wxCursor(wxCURSOR_ARROW));
  }

  if (dragMode != FrameDragMode::None && event.Dragging()) {
    SetCursor(CursorForMode(dragMode));
    wxPoint delta = currentPos - dragStartPos;
    wxPoint logicalDelta(static_cast<int>(std::lround(delta.x / zoom)),
                         static_cast<int>(std::lround(delta.y / zoom)));
    layouts::Layout2DViewFrame frame = dragStartFrame;
    if (dragMode == FrameDragMode::Move) {
      frame.x += logicalDelta.x;
      frame.y += logicalDelta.y;
    } else {
      if (dragMode == FrameDragMode::ResizeRight ||
          dragMode == FrameDragMode::ResizeCorner) {
        frame.width =
            std::max(kMinFrameSize, dragStartFrame.width + logicalDelta.x);
      }
      if (dragMode == FrameDragMode::ResizeBottom ||
          dragMode == FrameDragMode::ResizeCorner) {
        frame.height =
            std::max(kMinFrameSize, dragStartFrame.height + logicalDelta.y);
      }
    }
    UpdateFrame(frame, dragMode == FrameDragMode::Move);
    return;
  }

  if (!isPanning || !event.Dragging())
    return;

  wxPoint delta = currentPos - lastMousePos;
  panOffset += delta;
  lastMousePos = currentPos;
  Refresh();
}

void LayoutViewerPanel::OnMouseWheel(wxMouseEvent &event) {
  if (dragMode != FrameDragMode::None)
    return;
  const int rotation = event.GetWheelRotation();
  const int delta = event.GetWheelDelta();
  if (delta == 0 || rotation == 0)
    return;

  const double steps = static_cast<double>(rotation) /
                       static_cast<double>(delta);
  const double factor = std::pow(kZoomStep, steps);
  const double newZoom = std::clamp(zoom * factor, kMinZoom, kMaxZoom);
  if (std::abs(newZoom - zoom) < 1e-6)
    return;

  wxSize size = GetClientSize();
  wxPoint center(size.GetWidth() / 2, size.GetHeight() / 2);
  wxPoint mousePos = event.GetPosition();

  wxPoint relative = mousePos - center - panOffset;
  const double scale = newZoom / zoom;
  wxPoint newRelative(static_cast<int>(relative.x * scale),
                      static_cast<int>(relative.y * scale));

  panOffset += relative - newRelative;
  zoom = newZoom;
  Refresh();
}

void LayoutViewerPanel::OnCaptureLost(wxMouseCaptureLostEvent &) {
  isPanning = false;
  dragMode = FrameDragMode::None;
}

void LayoutViewerPanel::OnRightUp(wxMouseEvent &event) {
  const wxPoint pos = event.GetPosition();
  const layouts::Layout2DViewDefinition *view = GetEditableView();
  wxRect frameRect;
  if (!(view && GetFrameRect(view->frame, frameRect) &&
        frameRect.Contains(pos))) {
    event.Skip();
    return;
  }

  wxMenu menu;
  menu.Append(kEditMenuId, "2D View Editor");
  menu.Append(kDeleteMenuId, "Delete 2D View");
  PopupMenu(&menu, pos);
}

void LayoutViewerPanel::OnEditView(wxCommandEvent &) {
  EmitEditViewRequest();
}

void LayoutViewerPanel::OnDeleteView(wxCommandEvent &) {
  const layouts::Layout2DViewDefinition *view = GetEditableView();
  if (!view)
    return;
  const int viewIndex = view->camera.view;
  if (!currentLayout.name.empty()) {
    if (layouts::LayoutManager::Get().RemoveLayout2DView(currentLayout.name,
                                                        viewIndex)) {
      auto &views = currentLayout.view2dViews;
      views.erase(std::remove_if(views.begin(), views.end(),
                                 [viewIndex](const auto &entry) {
                                   return entry.camera.view == viewIndex;
                                 }),
                  views.end());
    }
  }
  Refresh();
}

void LayoutViewerPanel::ResetViewToFit() {
  wxSize size = GetClientSize();
  const double pageWidth = currentLayout.pageSetup.PageWidthPt();
  const double pageHeight = currentLayout.pageSetup.PageHeightPt();

  if (pageWidth <= 0.0 || pageHeight <= 0.0 || size.GetWidth() <= 0 ||
      size.GetHeight() <= 0) {
    zoom = 1.0;
    panOffset = wxPoint(0, 0);
    return;
  }

  const double fitWidth =
      static_cast<double>(size.GetWidth() - kFitMarginPx) / pageWidth;
  const double fitHeight =
      static_cast<double>(size.GetHeight() - kFitMarginPx) / pageHeight;
  zoom = std::clamp(std::min(fitWidth, fitHeight), kMinZoom, kMaxZoom);
  panOffset = wxPoint(0, 0);
}

wxRect LayoutViewerPanel::GetPageRect() const {
  wxSize size = GetClientSize();
  const double pageWidth = currentLayout.pageSetup.PageWidthPt();
  const double pageHeight = currentLayout.pageSetup.PageHeightPt();
  const double scaledWidth = pageWidth * zoom;
  const double scaledHeight = pageHeight * zoom;
  const wxPoint center(size.GetWidth() / 2, size.GetHeight() / 2);
  const wxPoint topLeft(center.x - static_cast<int>(scaledWidth / 2.0) +
                            panOffset.x,
                        center.y - static_cast<int>(scaledHeight / 2.0) +
                            panOffset.y);
  return wxRect(topLeft.x, topLeft.y, static_cast<int>(scaledWidth),
                static_cast<int>(scaledHeight));
}

bool LayoutViewerPanel::GetFrameRect(const layouts::Layout2DViewFrame &frame,
                                     wxRect &rect) const {
  if (frame.width <= 0 || frame.height <= 0)
    return false;
  wxRect pageRect = GetPageRect();
  const int scaledX = static_cast<int>(std::lround(frame.x * zoom));
  const int scaledY = static_cast<int>(std::lround(frame.y * zoom));
  const int scaledWidth = static_cast<int>(std::lround(frame.width * zoom));
  const int scaledHeight = static_cast<int>(std::lround(frame.height * zoom));
  rect = wxRect(pageRect.GetLeft() + scaledX, pageRect.GetTop() + scaledY,
                scaledWidth, scaledHeight);
  return true;
}

layouts::Layout2DViewDefinition *LayoutViewerPanel::GetEditableView() {
  if (!currentLayout.view2dViews.empty())
    return &currentLayout.view2dViews.front();
  return nullptr;
}

const layouts::Layout2DViewDefinition *LayoutViewerPanel::GetEditableView()
    const {
  if (!currentLayout.view2dViews.empty())
    return &currentLayout.view2dViews.front();
  return nullptr;
}

void LayoutViewerPanel::UpdateFrame(const layouts::Layout2DViewFrame &frame,
                                    bool updatePosition) {
  layouts::Layout2DViewDefinition *view = GetEditableView();
  if (!view)
    return;
  view->frame.width = frame.width;
  view->frame.height = frame.height;
  if (updatePosition) {
    view->frame.x = frame.x;
    view->frame.y = frame.y;
  }
  if (!currentLayout.name.empty()) {
    layouts::LayoutManager::Get().UpdateLayout2DView(currentLayout.name,
                                                     *view);
  }
  Refresh();
}

LayoutViewerPanel::FrameDragMode
LayoutViewerPanel::HitTestFrame(const wxPoint &pos,
                                const wxRect &frameRect) const {
  wxRect handleRight(frameRect.GetRight() - kHandleHalfPx - kHandleHoverPadPx,
                     frameRect.GetTop() + frameRect.GetHeight() / 2 -
                         kHandleHalfPx - kHandleHoverPadPx,
                     kHandleSizePx + kHandleHoverPadPx * 2,
                     kHandleSizePx + kHandleHoverPadPx * 2);
  wxRect handleBottom(frameRect.GetLeft() + frameRect.GetWidth() / 2 -
                          kHandleHalfPx - kHandleHoverPadPx,
                      frameRect.GetBottom() - kHandleHalfPx - kHandleHoverPadPx,
                      kHandleSizePx + kHandleHoverPadPx * 2,
                      kHandleSizePx + kHandleHoverPadPx * 2);
  wxRect handleCorner(frameRect.GetRight() - kHandleHalfPx - kHandleHoverPadPx,
                      frameRect.GetBottom() - kHandleHalfPx -
                          kHandleHoverPadPx,
                      kHandleSizePx + kHandleHoverPadPx * 2,
                      kHandleSizePx + kHandleHoverPadPx * 2);

  if (handleCorner.Contains(pos))
    return FrameDragMode::ResizeCorner;
  if (handleRight.Contains(pos))
    return FrameDragMode::ResizeRight;
  if (handleBottom.Contains(pos))
    return FrameDragMode::ResizeBottom;
  if (frameRect.Contains(pos))
    return FrameDragMode::Move;
  return FrameDragMode::None;
}

wxCursor LayoutViewerPanel::CursorForMode(FrameDragMode mode) const {
  switch (mode) {
  case FrameDragMode::ResizeRight:
    return wxCursor(wxCURSOR_SIZEWE);
  case FrameDragMode::ResizeBottom:
    return wxCursor(wxCURSOR_SIZENS);
  case FrameDragMode::ResizeCorner:
    return wxCursor(wxCURSOR_SIZENWSE);
  case FrameDragMode::Move:
    return wxCursor(wxCURSOR_SIZING);
  case FrameDragMode::None:
  default:
    return wxCursor(wxCURSOR_ARROW);
  }
}

void LayoutViewerPanel::EmitEditViewRequest() {
  wxCommandEvent event(EVT_LAYOUT_VIEW_EDIT);
  event.SetEventObject(this);
  ProcessWindowEvent(event);
}
