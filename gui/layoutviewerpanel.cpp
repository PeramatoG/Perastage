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
#include <memory>
#include <wx/dcbuffer.h>
#include <wx/graphics.h>

#include "configmanager.h"
#include "layouts/LayoutManager.h"
#include "viewer2dcommandrenderer.h"
#include "viewer2dstate.h"

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
wxColour ToWxColor(const CanvasColor &color) {
  auto clamp = [](float v) {
    return static_cast<unsigned char>(
        std::clamp(v, 0.0f, 1.0f) * 255.0f);
  };
  return wxColour(clamp(color.r), clamp(color.g), clamp(color.b),
                  clamp(color.a));
}

class WxGraphicsCommandBackend : public viewer2d::IViewer2DCommandBackend {
public:
  explicit WxGraphicsCommandBackend(wxGraphicsContext &gc) : gc_(gc) {}

  void DrawLine(const viewer2d::Viewer2DRenderPoint &p0,
                const viewer2d::Viewer2DRenderPoint &p1,
                const CanvasStroke &stroke, double strokeWidthPx) override {
    wxPen pen(ToWxColor(stroke.color),
              std::max(1, static_cast<int>(std::lround(strokeWidthPx))));
    gc_.SetPen(pen);
    gc_.StrokeLine(p0.x, p0.y, p1.x, p1.y);
  }

  void DrawPolyline(const std::vector<viewer2d::Viewer2DRenderPoint> &points,
                    const CanvasStroke &stroke,
                    double strokeWidthPx) override {
    if (points.size() < 2)
      return;
    wxGraphicsPath path = gc_.CreatePath();
    path.MoveToPoint(points.front().x, points.front().y);
    for (size_t i = 1; i < points.size(); ++i) {
      path.AddLineToPoint(points[i].x, points[i].y);
    }
    wxPen pen(ToWxColor(stroke.color),
              std::max(1, static_cast<int>(std::lround(strokeWidthPx))));
    gc_.SetPen(pen);
    gc_.StrokePath(path);
  }

  void DrawPolygon(const std::vector<viewer2d::Viewer2DRenderPoint> &points,
                   const CanvasStroke &stroke, const CanvasFill *fill,
                   double strokeWidthPx) override {
    if (points.size() < 3)
      return;
    wxGraphicsPath path = gc_.CreatePath();
    path.MoveToPoint(points.front().x, points.front().y);
    for (size_t i = 1; i < points.size(); ++i) {
      path.AddLineToPoint(points[i].x, points[i].y);
    }
    path.CloseSubpath();
    if (fill) {
      gc_.SetBrush(wxBrush(ToWxColor(fill->color)));
      gc_.FillPath(path);
    }
    wxPen pen(ToWxColor(stroke.color),
              std::max(1, static_cast<int>(std::lround(strokeWidthPx))));
    gc_.SetPen(pen);
    gc_.StrokePath(path);
  }

  void DrawCircle(const viewer2d::Viewer2DRenderPoint &center, double radiusPx,
                  const CanvasStroke &stroke, const CanvasFill *fill,
                  double strokeWidthPx) override {
    wxRect2DDouble rect(center.x - radiusPx, center.y - radiusPx,
                        radiusPx * 2.0, radiusPx * 2.0);
    if (fill) {
      gc_.SetBrush(wxBrush(ToWxColor(fill->color)));
      gc_.DrawEllipse(rect.m_x, rect.m_y, rect.m_width, rect.m_height);
    }
    wxPen pen(ToWxColor(stroke.color),
              std::max(1, static_cast<int>(std::lround(strokeWidthPx))));
    gc_.SetPen(pen);
    gc_.SetBrush(*wxTRANSPARENT_BRUSH);
    gc_.DrawEllipse(rect.m_x, rect.m_y, rect.m_width, rect.m_height);
  }

  void DrawText(const viewer2d::Viewer2DRenderText &text) override {
    int pixelSize = std::max(1, static_cast<int>(std::lround(text.fontSizePx)));
    wxFontInfo fontInfo(pixelSize);
    if (!text.style.fontFamily.empty())
      fontInfo.FaceName(wxString::FromUTF8(text.style.fontFamily));
    wxFont font(fontInfo);
    gc_.SetFont(font, ToWxColor(text.style.color));

    if (text.outlineWidthPx > 0.0) {
      double outline = text.outlineWidthPx;
      gc_.SetFont(font, ToWxColor(text.style.outlineColor));
      const std::array<wxPoint2DDouble, 4> offsets = {
          wxPoint2DDouble{-outline, 0.0},
          wxPoint2DDouble{outline, 0.0},
          wxPoint2DDouble{0.0, -outline},
          wxPoint2DDouble{0.0, outline}};
      for (const auto &offset : offsets) {
        DrawTextLines(wxString::FromUTF8(text.text),
                      wxPoint2DDouble(text.anchor.x + offset.m_x,
                                      text.anchor.y + offset.m_y),
                      text.lineHeightPx, text.style.hAlign,
                      text.style.vAlign);
      }
      gc_.SetFont(font, ToWxColor(text.style.color));
    }

    DrawTextLines(wxString::FromUTF8(text.text),
                  wxPoint2DDouble(text.anchor.x, text.anchor.y),
                  text.lineHeightPx, text.style.hAlign, text.style.vAlign);
  }

private:
  void DrawTextLines(const wxString &text, const wxPoint2DDouble &anchor,
                     double lineHeight,
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
      gc_.GetTextExtent(line, &w, &h, &descent, &externalLeading);
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
      gc_.DrawText(lines[i], x, y + lineHeight * static_cast<double>(i));
    }
  }

  wxGraphicsContext &gc_;
};
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

void LayoutViewerPanel::SetCapturePanel(Viewer2DPanel *panel) {
  if (capturePanel.get() == panel)
    return;
  capturePanel = panel;
  captureInProgress = false;
  captureVersion = -1;
  hasCapture = false;
  cachedBuffer = CommandBuffer{};
  cachedSymbols.reset();
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
    Viewer2DPanel *panel = capturePanel.get();
    if (panel) {
      captureInProgress = true;
      const int fallbackViewportWidth =
          view->camera.viewportWidth > 0 ? view->camera.viewportWidth
                                         : view->frame.width;
      const int fallbackViewportHeight =
          view->camera.viewportHeight > 0 ? view->camera.viewportHeight
                                          : view->frame.height;
      ConfigManager &cfg = ConfigManager::Get();
      viewer2d::Viewer2DState layoutState =
          viewer2d::FromLayoutDefinition(*view);
      auto stateGuard = std::make_shared<viewer2d::ScopedViewer2DState>(
          panel, nullptr, cfg, layoutState, panel, nullptr);
      wxWeakRef<Viewer2DPanel> panelRef(panel);
      panel->CaptureFrameAsync(
          [this, panelRef, stateGuard, fallbackViewportWidth,
           fallbackViewportHeight](
              CommandBuffer buffer, Viewer2DViewState state) {
            Viewer2DPanel *panel = panelRef.get();
            if (!panel) {
              captureInProgress = false;
              return;
            }
            cachedBuffer = std::move(buffer);
            cachedViewState = state;
            if (fallbackViewportWidth > 0) {
              cachedViewState.viewportWidth = fallbackViewportWidth;
            }
            if (fallbackViewportHeight > 0) {
              cachedViewState.viewportHeight = fallbackViewportHeight;
            }
            cachedSymbols.reset();
            cachedSymbols = panel->GetBottomSymbolCacheSnapshot();
            hasCapture = !cachedBuffer.commands.empty();
            captureVersion = layoutVersion;
            captureInProgress = false;
            Refresh();
          });
      panel->Refresh();
      panel->Update();
    }
  }

  if (hasCapture && frameRect.GetWidth() > 0 && frameRect.GetHeight() > 0) {
    Viewer2DViewState renderState = cachedViewState;
    if (renderState.viewportWidth <= 0) {
      if (view->camera.viewportWidth > 0) {
        renderState.viewportWidth = view->camera.viewportWidth;
      } else {
        renderState.viewportWidth = view->frame.width;
      }
    }
    if (renderState.viewportHeight <= 0) {
      if (view->camera.viewportHeight > 0) {
        renderState.viewportHeight = view->camera.viewportHeight;
      } else {
        renderState.viewportHeight = view->frame.height;
      }
    }
    wxBitmap bufferBitmap(frameRect.GetWidth(), frameRect.GetHeight());
    wxMemoryDC memDC(bufferBitmap);
    memDC.SetBackground(wxBrush(wxColour(255, 255, 255)));
    memDC.Clear();
    if (auto gc = wxGraphicsContext::Create(memDC)) {
      viewer2d::Viewer2DRenderMapping mapping;
      if (viewer2d::BuildViewMapping(
              renderState, frameRect.GetWidth(), frameRect.GetHeight(), 0.0,
              mapping)) {
        WxGraphicsCommandBackend backend(*gc);
        viewer2d::Viewer2DCommandRenderer renderer(mapping, backend,
                                                   cachedSymbols.get());
        renderer.Render(cachedBuffer);
      }
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
