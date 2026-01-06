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
#include <filesystem>
#include <limits>
#include <map>
#include <memory>
#include <vector>

#include <GL/gl.h>
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

#include "configmanager.h"
#include "gdtfloader.h"
#include "legendutils.h"
#include "layouts/LayoutManager.h"
#include "mainwindow.h"
#include "viewer2dcommandrenderer.h"
#include "viewer2doffscreenrenderer.h"
#include "viewer2dstate.h"
#include <wx/filename.h>

namespace {
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
  explicit LegendSymbolBackend(wxDC &dc) : dc_(dc) {}

  void DrawLine(const viewer2d::Viewer2DRenderPoint &p0,
                const viewer2d::Viewer2DRenderPoint &p1,
                const CanvasStroke &stroke, double strokeWidthPx) override {
    if (strokeWidthPx <= 0.0)
      return;
    wxPen pen(ToWxColor(stroke.color),
              std::max(1, (int)std::lround(strokeWidthPx)));
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
    if (strokeWidthPx <= 0.0)
      return;
    wxPen pen(ToWxColor(stroke.color),
              std::max(1, (int)std::lround(strokeWidthPx)));
    dc_.SetPen(pen);
    dc_.SetBrush(*wxTRANSPARENT_BRUSH);
    std::vector<wxPoint> wxPoints;
    wxPoints.reserve(points.size());
    for (const auto &pt : points) {
      wxPoints.emplace_back(std::lround(pt.x), std::lround(pt.y));
    }
    dc_.DrawLines(static_cast<int>(wxPoints.size()), wxPoints.data());
  }

  void DrawPolygon(const std::vector<viewer2d::Viewer2DRenderPoint> &points,
                   const CanvasStroke &stroke, const CanvasFill *fill,
                   double strokeWidthPx) override {
    if (points.empty())
      return;
    if (strokeWidthPx > 0.0) {
      wxPen pen(ToWxColor(stroke.color),
                std::max(1, (int)std::lround(strokeWidthPx)));
      dc_.SetPen(pen);
    } else {
      dc_.SetPen(*wxTRANSPARENT_PEN);
    }
    if (fill) {
      dc_.SetBrush(wxBrush(ToWxColor(fill->color)));
    } else {
      dc_.SetBrush(*wxTRANSPARENT_BRUSH);
    }
    std::vector<wxPoint> wxPoints;
    wxPoints.reserve(points.size());
    for (const auto &pt : points) {
      wxPoints.emplace_back(std::lround(pt.x), std::lround(pt.y));
    }
    dc_.DrawPolygon(static_cast<int>(wxPoints.size()), wxPoints.data());
  }

  void DrawCircle(const viewer2d::Viewer2DRenderPoint &center, double radiusPx,
                  const CanvasStroke &stroke, const CanvasFill *fill,
                  double strokeWidthPx) override {
    if (strokeWidthPx > 0.0) {
      wxPen pen(ToWxColor(stroke.color),
                std::max(1, (int)std::lround(strokeWidthPx)));
      dc_.SetPen(pen);
    } else {
      dc_.SetPen(*wxTRANSPARENT_PEN);
    }
    if (fill) {
      dc_.SetBrush(wxBrush(ToWxColor(fill->color)));
    } else {
      dc_.SetBrush(*wxTRANSPARENT_BRUSH);
    }
    dc_.DrawCircle(wxPoint(std::lround(center.x), std::lround(center.y)),
                   std::lround(radiusPx));
  }

  void DrawText(const viewer2d::Viewer2DRenderText &text) override {
    (void)text;
  }

private:
  wxDC &dc_;
};
constexpr double kMinZoom = 0.1;
constexpr double kMaxZoom = 10.0;
constexpr double kZoomStep = 1.1;
constexpr int kZoomCacheStepsPerLevel = 2;
constexpr int kFitMarginPx = 40;
constexpr int kHandleSizePx = 10;
constexpr int kHandleHalfPx = kHandleSizePx / 2;
constexpr int kHandleHoverPadPx = 6;
constexpr int kMinFrameSize = 24;
constexpr int kEditMenuId = wxID_HIGHEST + 490;
constexpr int kDeleteMenuId = wxID_HIGHEST + 491;
constexpr int kDeleteLegendMenuId = wxID_HIGHEST + 492;
}

wxDEFINE_EVENT(EVT_LAYOUT_VIEW_EDIT, wxCommandEvent);

wxBEGIN_EVENT_TABLE(LayoutViewerPanel, wxGLCanvas)
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
    EVT_MENU(kDeleteLegendMenuId, LayoutViewerPanel::OnDeleteLegend)
wxEND_EVENT_TABLE()

LayoutViewerPanel::LayoutViewerPanel(wxWindow *parent)
    : wxGLCanvas(parent, wxID_ANY, nullptr, wxDefaultPosition,
                 wxDefaultSize, wxFULL_REPAINT_ON_RESIZE) {
  SetBackgroundStyle(wxBG_STYLE_CUSTOM);
  glContext_ = new wxGLContext(this);
  currentLayout.pageSetup.pageSize = print::PageSize::A4;
  currentLayout.pageSetup.landscape = false;
  ResetViewToFit();
}

LayoutViewerPanel::~LayoutViewerPanel() {
  ClearCachedTexture();
  delete glContext_;
}

void LayoutViewerPanel::SetLayoutDefinition(
    const layouts::LayoutDefinition &layout) {
  currentLayout = layout;
  if (!currentLayout.view2dViews.empty()) {
    selectedElementType = SelectedElementType::View2D;
    selectedElementId = currentLayout.view2dViews.front().id;
  } else if (!currentLayout.legendViews.empty()) {
    selectedElementType = SelectedElementType::Legend;
    selectedElementId = currentLayout.legendViews.front().id;
  } else {
    selectedElementType = SelectedElementType::None;
    selectedElementId = -1;
  }
  layoutVersion++;
  captureInProgress = false;
  ClearCachedTexture();
  renderDirty = true;
  RefreshLegendData();
  ResetViewToFit();
  RequestRenderRebuild();
  Refresh();
}

void LayoutViewerPanel::OnPaint(wxPaintEvent &) {
  wxPaintDC dc(this);
  InitGL();
  SetCurrent(*glContext_);
  RefreshLegendData();

  wxSize size = GetClientSize();
  glViewport(0, 0, size.GetWidth(), size.GetHeight());
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0.0, size.GetWidth(), size.GetHeight(), 0.0, -1.0, 1.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glDisable(GL_DEPTH_TEST);
  glClearColor(0.35f, 0.35f, 0.35f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  const double pageWidth = currentLayout.pageSetup.PageWidthPt();
  const double pageHeight = currentLayout.pageSetup.PageHeightPt();

  const double scaledWidth = pageWidth * zoom;
  const double scaledHeight = pageHeight * zoom;

  const wxPoint center(size.GetWidth() / 2, size.GetHeight() / 2);
  const wxPoint topLeft(center.x - static_cast<int>(scaledWidth / 2.0) +
                            panOffset.x,
                        center.y - static_cast<int>(scaledHeight / 2.0) +
                            panOffset.y);

  glColor4ub(255, 255, 255, 255);
  glBegin(GL_QUADS);
  glVertex2f(static_cast<float>(topLeft.x), static_cast<float>(topLeft.y));
  glVertex2f(static_cast<float>(topLeft.x + scaledWidth),
             static_cast<float>(topLeft.y));
  glVertex2f(static_cast<float>(topLeft.x + scaledWidth),
             static_cast<float>(topLeft.y + scaledHeight));
  glVertex2f(static_cast<float>(topLeft.x),
             static_cast<float>(topLeft.y + scaledHeight));
  glEnd();

  glColor4ub(200, 200, 200, 255);
  glLineWidth(1.0f);
  glBegin(GL_LINE_LOOP);
  glVertex2f(static_cast<float>(topLeft.x), static_cast<float>(topLeft.y));
  glVertex2f(static_cast<float>(topLeft.x + scaledWidth),
             static_cast<float>(topLeft.y));
  glVertex2f(static_cast<float>(topLeft.x + scaledWidth),
             static_cast<float>(topLeft.y + scaledHeight));
  glVertex2f(static_cast<float>(topLeft.x),
             static_cast<float>(topLeft.y + scaledHeight));
  glEnd();

  const layouts::Layout2DViewDefinition *activeView =
      static_cast<const LayoutViewerPanel *>(this)->GetEditableView();
  const int activeViewId =
      selectedElementType == SelectedElementType::View2D && activeView
          ? activeView->id
          : -1;
  const int activeLegendId =
      selectedElementType == SelectedElementType::Legend ? selectedElementId
                                                         : -1;

  auto drawSelectionHandles = [](const wxRect &frameRect) {
    wxRect handleRight(frameRect.GetRight() - kHandleHalfPx,
                       frameRect.GetTop() + frameRect.GetHeight() / 2 -
                           kHandleHalfPx,
                       kHandleSizePx, kHandleSizePx);
    wxRect handleBottom(frameRect.GetLeft() + frameRect.GetWidth() / 2 -
                            kHandleHalfPx,
                        frameRect.GetBottom() - kHandleHalfPx,
                        kHandleSizePx, kHandleSizePx);
    wxRect handleCorner(frameRect.GetRight() - kHandleHalfPx,
                        frameRect.GetBottom() - kHandleHalfPx,
                        kHandleSizePx, kHandleSizePx);

    glColor4ub(60, 160, 240, 255);
    auto drawHandle = [](const wxRect &rect) {
      glBegin(GL_QUADS);
      glVertex2f(static_cast<float>(rect.GetLeft()),
                 static_cast<float>(rect.GetTop()));
      glVertex2f(static_cast<float>(rect.GetRight()),
                 static_cast<float>(rect.GetTop()));
      glVertex2f(static_cast<float>(rect.GetRight()),
                 static_cast<float>(rect.GetBottom()));
      glVertex2f(static_cast<float>(rect.GetLeft()),
                 static_cast<float>(rect.GetBottom()));
      glEnd();
    };
    drawHandle(handleRight);
    drawHandle(handleBottom);
    drawHandle(handleCorner);
  };

  Viewer2DPanel *capturePanel = nullptr;
  Viewer2DOffscreenRenderer *offscreenRenderer = nullptr;
  if (auto *mw = MainWindow::Instance()) {
    offscreenRenderer = mw->GetOffscreenRenderer();
    capturePanel =
        offscreenRenderer ? offscreenRenderer->GetPanel() : nullptr;
  } else {
    capturePanel = Viewer2DPanel::Instance();
  }

  for (const auto &view : currentLayout.view2dViews) {
    ViewCache &cache = GetViewCache(view.id);
    if (!captureInProgress && !cache.captureInProgress &&
        cache.captureVersion != layoutVersion && capturePanel) {
      captureInProgress = true;
      cache.captureInProgress = true;
      const int viewId = view.id;
      const int fallbackViewportWidth = view.camera.viewportWidth > 0
                                            ? view.camera.viewportWidth
                                            : view.frame.width;
      const int fallbackViewportHeight = view.camera.viewportHeight > 0
                                             ? view.camera.viewportHeight
                                             : view.frame.height;
      ConfigManager &cfg = ConfigManager::Get();
      viewer2d::Viewer2DState layoutState =
          viewer2d::FromLayoutDefinition(view);
      layoutState.renderOptions.darkMode = false;
      cache.renderState = layoutState;
      cache.hasRenderState = true;
      if (offscreenRenderer && fallbackViewportWidth > 0 &&
          fallbackViewportHeight > 0) {
        offscreenRenderer->SetViewportSize(
            wxSize(fallbackViewportWidth, fallbackViewportHeight));
        offscreenRenderer->PrepareForCapture();
      }
      auto stateGuard = std::make_shared<viewer2d::ScopedViewer2DState>(
          capturePanel, nullptr, cfg, layoutState);
      capturePanel->CaptureFrameNow(
          [this, viewId, stateGuard, fallbackViewportWidth,
           fallbackViewportHeight, capturePanel](
              CommandBuffer buffer, Viewer2DViewState state) {
            ViewCache &cache = GetViewCache(viewId);
            cache.buffer = std::move(buffer);
            cache.viewState = state;
            if (cache.viewState.viewportWidth <= 0 &&
                fallbackViewportWidth > 0) {
              cache.viewState.viewportWidth = fallbackViewportWidth;
            }
            if (cache.viewState.viewportHeight <= 0 &&
                fallbackViewportHeight > 0) {
              cache.viewState.viewportHeight = fallbackViewportHeight;
            }
            cache.symbols.reset();
            if (capturePanel) {
              cache.symbols =
                  capturePanel->GetBottomSymbolCacheSnapshot();
            }
            cache.hasCapture = !cache.buffer.commands.empty();
            cache.captureVersion = layoutVersion;
            cache.captureInProgress = false;
            captureInProgress = false;
            cache.renderDirty = true;
            renderDirty = true;
            cache.textureSize = wxSize(0, 0);
            cache.renderZoom = 0.0;
            RequestRenderRebuild();
            Refresh();
          });
    }

    wxRect frameRect;
    if (!GetFrameRect(view.frame, frameRect))
      continue;
    const int frameRight = frameRect.GetLeft() + frameRect.GetWidth();
    const int frameBottom = frameRect.GetTop() + frameRect.GetHeight();

    const wxSize renderSize =
        GetFrameSizeForZoom(view.frame, cache.renderZoom);
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
      glColor4ub(240, 240, 240, 255);
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

    if (view.id == activeViewId) {
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

    if (view.id == activeViewId)
      drawSelectionHandles(frameRect);
  }

  for (const auto &legend : currentLayout.legendViews) {
    LegendCache &cache = GetLegendCache(legend.id);
    wxRect frameRect;
    if (!GetFrameRect(legend.frame, frameRect))
      continue;
    const int frameRight = frameRect.GetLeft() + frameRect.GetWidth();
    const int frameBottom = frameRect.GetTop() + frameRect.GetHeight();

    const wxSize renderSize =
        GetFrameSizeForZoom(legend.frame, cache.renderZoom);
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
      drawSelectionHandles(frameRect);
  }

  glFlush();
  SwapBuffers();
}

void LayoutViewerPanel::OnSize(wxSizeEvent &) {
  InvalidateRenderIfFrameChanged();
  RequestRenderRebuild();
  Refresh();
}

void LayoutViewerPanel::OnLeftDown(wxMouseEvent &event) {
  const wxPoint pos = event.GetPosition();
  SelectElementAtPosition(pos);
  layouts::Layout2DViewFrame selectedFrame;
  wxRect frameRect;
  if (GetSelectedFrame(selectedFrame) &&
      GetFrameRect(selectedFrame, frameRect)) {
    FrameDragMode mode = HitTestFrame(pos, frameRect);
    if (mode != FrameDragMode::None) {
      dragMode = mode;
      dragStartPos = pos;
      dragStartFrame = selectedFrame;
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
  SelectElementAtPosition(pos);
  const layouts::Layout2DViewDefinition *view = GetEditableView();
  wxRect frameRect;
  if (view && GetFrameRect(view->frame, frameRect) && frameRect.Contains(pos) &&
      selectedElementType == SelectedElementType::View2D) {
    EmitEditViewRequest();
    return;
  }
  event.Skip();
}

void LayoutViewerPanel::OnMouseMove(wxMouseEvent &event) {
  wxPoint currentPos = event.GetPosition();
  if (dragMode == FrameDragMode::None && !isPanning) {
    SelectElementAtPosition(currentPos);
  }
  layouts::Layout2DViewFrame selectedFrame;
  wxRect frameRect;
  if (GetSelectedFrame(selectedFrame) &&
      GetFrameRect(selectedFrame, frameRect)) {
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
    if (selectedElementType == SelectedElementType::Legend) {
      UpdateLegendFrame(frame, dragMode == FrameDragMode::Move);
    } else {
      UpdateFrame(frame, dragMode == FrameDragMode::Move);
    }
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
  InvalidateRenderIfFrameChanged();
  RequestRenderRebuild();
  Refresh();
}

void LayoutViewerPanel::OnCaptureLost(wxMouseCaptureLostEvent &) {
  isPanning = false;
  dragMode = FrameDragMode::None;
}

void LayoutViewerPanel::OnRightUp(wxMouseEvent &event) {
  const wxPoint pos = event.GetPosition();
  if (!SelectElementAtPosition(pos)) {
    event.Skip();
    return;
  }
  layouts::Layout2DViewFrame selectedFrame;
  wxRect frameRect;
  if (!(GetSelectedFrame(selectedFrame) &&
        GetFrameRect(selectedFrame, frameRect) && frameRect.Contains(pos))) {
    event.Skip();
    return;
  }

  wxMenu menu;
  if (selectedElementType == SelectedElementType::View2D) {
    menu.Append(kEditMenuId, "2D View Editor");
    menu.Append(kDeleteMenuId, "Delete 2D View");
  } else if (selectedElementType == SelectedElementType::Legend) {
    menu.Append(kDeleteLegendMenuId, "Delete Legend");
  }
  PopupMenu(&menu, pos);
}

void LayoutViewerPanel::OnEditView(wxCommandEvent &) {
  if (selectedElementType != SelectedElementType::View2D)
    return;
  EmitEditViewRequest();
}

void LayoutViewerPanel::OnDeleteView(wxCommandEvent &) {
  if (selectedElementType != SelectedElementType::View2D)
    return;
  const layouts::Layout2DViewDefinition *view = GetEditableView();
  if (!view)
    return;
  const int viewId = view->id;
  if (!currentLayout.name.empty()) {
    if (layouts::LayoutManager::Get().RemoveLayout2DView(currentLayout.name,
                                                        viewId)) {
      auto &views = currentLayout.view2dViews;
      views.erase(std::remove_if(views.begin(), views.end(),
                                 [viewId](const auto &entry) {
                                   return entry.id == viewId;
                                 }),
                  views.end());
      if (selectedElementType == SelectedElementType::View2D &&
          selectedElementId == viewId) {
        if (!views.empty()) {
          selectedElementType = SelectedElementType::View2D;
          selectedElementId = views.front().id;
        } else if (!currentLayout.legendViews.empty()) {
          selectedElementType = SelectedElementType::Legend;
          selectedElementId = currentLayout.legendViews.front().id;
        } else {
          selectedElementType = SelectedElementType::None;
          selectedElementId = -1;
        }
      }
    }
  }
  auto cacheIt = viewCaches_.find(viewId);
  if (cacheIt != viewCaches_.end()) {
    ClearCachedTexture(cacheIt->second);
    viewCaches_.erase(cacheIt);
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
  InvalidateRenderIfFrameChanged();
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

wxSize LayoutViewerPanel::GetFrameSizeForZoom(
    const layouts::Layout2DViewFrame &frame, double targetZoom) const {
  if (frame.width <= 0 || frame.height <= 0 || targetZoom <= 0.0)
    return wxSize(0, 0);
  const int scaledWidth =
      static_cast<int>(std::lround(frame.width * targetZoom));
  const int scaledHeight =
      static_cast<int>(std::lround(frame.height * targetZoom));
  return wxSize(scaledWidth, scaledHeight);
}

double LayoutViewerPanel::GetRenderZoom() const {
  return zoom;
}

layouts::Layout2DViewDefinition *LayoutViewerPanel::GetEditableView() {
  if (currentLayout.view2dViews.empty())
    return nullptr;
  if (selectedElementType == SelectedElementType::View2D &&
      selectedElementId >= 0) {
    for (auto &view : currentLayout.view2dViews) {
      if (view.id == selectedElementId)
        return &view;
    }
  }
  selectedElementType = SelectedElementType::View2D;
  selectedElementId = currentLayout.view2dViews.front().id;
  return &currentLayout.view2dViews.front();
}

const layouts::Layout2DViewDefinition *LayoutViewerPanel::GetEditableView()
    const {
  if (currentLayout.view2dViews.empty())
    return nullptr;
  if (selectedElementType == SelectedElementType::View2D &&
      selectedElementId >= 0) {
    for (const auto &view : currentLayout.view2dViews) {
      if (view.id == selectedElementId)
        return &view;
    }
  }
  if (!currentLayout.view2dViews.empty())
    return &currentLayout.view2dViews.front();
  return nullptr;
}

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

bool LayoutViewerPanel::GetSelectedFrame(
    layouts::Layout2DViewFrame &frame) const {
  if (selectedElementType == SelectedElementType::Legend) {
    const auto *legend = GetSelectedLegend();
    if (!legend)
      return false;
    frame = legend->frame;
    return true;
  }
  const auto *view = GetEditableView();
  if (!view)
    return false;
  frame = view->frame;
  return true;
}

void LayoutViewerPanel::UpdateFrame(const layouts::Layout2DViewFrame &frame,
                                    bool updatePosition) {
  layouts::Layout2DViewDefinition *view = GetEditableView();
  if (!view)
    return;
  const bool sizeChanged =
      view->frame.width != frame.width || view->frame.height != frame.height;
  view->frame.width = frame.width;
  view->frame.height = frame.height;
  if (updatePosition) {
    view->frame.x = frame.x;
    view->frame.y = frame.y;
  }
  if (sizeChanged) {
    if (frame.width > 0) {
      view->camera.viewportWidth = frame.width;
    } else {
      view->camera.viewportWidth = 0;
    }
    if (frame.height > 0) {
      view->camera.viewportHeight = frame.height;
    } else {
      view->camera.viewportHeight = 0;
    }
  }
  if (!currentLayout.name.empty()) {
    layouts::LayoutManager::Get().UpdateLayout2DView(currentLayout.name,
                                                     *view);
  }
  InvalidateRenderIfFrameChanged();
  RequestRenderRebuild();
  Refresh();
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
  RequestRenderRebuild();
  Refresh();
}

void LayoutViewerPanel::InitGL() {
  if (!glContext_)
    return;
  SetCurrent(*glContext_);
  if (!glInitialized_) {
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glInitialized_ = true;
  }
}

void LayoutViewerPanel::RebuildCachedTexture() {
  if (!renderDirty)
    return;

  renderDirty = false;

  Viewer2DOffscreenRenderer *offscreenRenderer = nullptr;
  Viewer2DPanel *capturePanel = nullptr;
  if (auto *mw = MainWindow::Instance()) {
    offscreenRenderer = mw->GetOffscreenRenderer();
    capturePanel = offscreenRenderer ? offscreenRenderer->GetPanel() : nullptr;
  }
  if (!capturePanel || !offscreenRenderer) {
    ClearCachedTexture();
    return;
  }

  std::shared_ptr<const SymbolDefinitionSnapshot> legendSymbols =
      capturePanel->GetBottomSymbolCacheSnapshot();
  if ((!legendSymbols || legendSymbols->empty()) &&
      !currentLayout.legendViews.empty()) {
    capturePanel->CaptureFrameNow(
        [](CommandBuffer, Viewer2DViewState) {}, true, false);
    legendSymbols = capturePanel->GetBottomSymbolCacheSnapshot();
  }
  const double renderZoom = GetRenderZoom();
  for (const auto &view : currentLayout.view2dViews) {
    ViewCache &cache = GetViewCache(view.id);
    if (!cache.renderDirty)
      continue;
    cache.renderDirty = false;
    wxRect frameRect;
    if (!cache.hasCapture || !cache.hasRenderState ||
        !GetFrameRect(view.frame, frameRect)) {
      ClearCachedTexture(cache);
      cache.textureSize = wxSize(0, 0);
      cache.renderZoom = 0.0;
      continue;
    }

    const wxSize renderSize = GetFrameSizeForZoom(view.frame, renderZoom);
    if (renderSize.GetWidth() <= 0 || renderSize.GetHeight() <= 0) {
      ClearCachedTexture(cache);
      cache.textureSize = wxSize(0, 0);
      cache.renderZoom = 0.0;
      continue;
    }

    offscreenRenderer->SetViewportSize(renderSize);
    offscreenRenderer->PrepareForCapture();

    ConfigManager &cfg = ConfigManager::Get();
    viewer2d::Viewer2DState renderState = cache.renderState;
    if (renderZoom != 1.0) {
      renderState.camera.zoom *= static_cast<float>(renderZoom);
    }
    renderState.camera.viewportWidth = renderSize.GetWidth();
    renderState.camera.viewportHeight = renderSize.GetHeight();

    auto stateGuard = std::make_shared<viewer2d::ScopedViewer2DState>(
        capturePanel, nullptr, cfg, renderState);

    std::vector<unsigned char> pixels;
    int width = 0;
    int height = 0;
    if (!capturePanel->RenderToRGBA(pixels, width, height) || width <= 0 ||
        height <= 0) {
      ClearCachedTexture(cache);
      cache.textureSize = wxSize(0, 0);
      cache.renderZoom = 0.0;
      continue;
    }

    InitGL();
    SetCurrent(*glContext_);
    if (cache.texture == 0) {
      glGenTextures(1, &cache.texture);
    }
    glBindTexture(GL_TEXTURE_2D, cache.texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, pixels.data());
    cache.textureSize = wxSize(width, height);
    cache.renderZoom = renderZoom;
  }

  for (const auto &legend : currentLayout.legendViews) {
    LegendCache &cache = GetLegendCache(legend.id);
    if (cache.symbols != legendSymbols) {
      cache.symbols = legendSymbols;
      cache.renderDirty = true;
    }
    if (cache.contentHash != legendDataHash) {
      cache.renderDirty = true;
    }
    if (!cache.renderDirty)
      continue;
    cache.renderDirty = false;

    const wxSize renderSize = GetFrameSizeForZoom(legend.frame, renderZoom);
    if (renderSize.GetWidth() <= 0 || renderSize.GetHeight() <= 0) {
      ClearCachedTexture(cache);
      cache.textureSize = wxSize(0, 0);
      cache.renderZoom = 0.0;
      continue;
    }

    wxImage image =
        BuildLegendImage(renderSize, legendItems_, cache.symbols.get());
    if (!image.IsOk()) {
      ClearCachedTexture(cache);
      cache.textureSize = wxSize(0, 0);
      cache.renderZoom = 0.0;
      continue;
    }
    image = image.Mirror(false);
    if (!image.HasAlpha())
      image.InitAlpha();
    const int width = image.GetWidth();
    const int height = image.GetHeight();
    const unsigned char *rgb = image.GetData();
    const unsigned char *alpha = image.GetAlpha();
    if (!rgb || width <= 0 || height <= 0) {
      ClearCachedTexture(cache);
      cache.textureSize = wxSize(0, 0);
      cache.renderZoom = 0.0;
      continue;
    }

    std::vector<unsigned char> pixels;
    pixels.resize(static_cast<size_t>(width) * height * 4);
    for (int i = 0; i < width * height; ++i) {
      pixels[static_cast<size_t>(i) * 4] = rgb[i * 3];
      pixels[static_cast<size_t>(i) * 4 + 1] = rgb[i * 3 + 1];
      pixels[static_cast<size_t>(i) * 4 + 2] = rgb[i * 3 + 2];
      pixels[static_cast<size_t>(i) * 4 + 3] = alpha ? alpha[i] : 255;
    }

    InitGL();
    SetCurrent(*glContext_);
    if (cache.texture == 0) {
      glGenTextures(1, &cache.texture);
    }
    glBindTexture(GL_TEXTURE_2D, cache.texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, pixels.data());
    cache.textureSize = wxSize(width, height);
    cache.renderZoom = renderZoom;
    cache.contentHash = legendDataHash;
  }
}

void LayoutViewerPanel::ClearCachedTexture() {
  for (auto &entry : viewCaches_) {
    ClearCachedTexture(entry.second);
  }
  viewCaches_.clear();
  for (auto &entry : legendCaches_) {
    ClearCachedTexture(entry.second);
  }
  legendCaches_.clear();
}

void LayoutViewerPanel::ClearCachedTexture(ViewCache &cache) {
  if (cache.texture == 0 || !glContext_)
    return;
  if (!IsShown()) {
    cache.texture = 0;
    return;
  }
  SetCurrent(*glContext_);
  glDeleteTextures(1, &cache.texture);
  cache.texture = 0;
}

void LayoutViewerPanel::ClearCachedTexture(LegendCache &cache) {
  if (cache.texture == 0 || !glContext_)
    return;
  if (!IsShown()) {
    cache.texture = 0;
    return;
  }
  SetCurrent(*glContext_);
  glDeleteTextures(1, &cache.texture);
  cache.texture = 0;
}

void LayoutViewerPanel::RequestRenderRebuild() {
  if (!renderDirty || renderPending)
    return;
  renderPending = true;
  CallAfter([this]() {
    renderPending = false;
    RebuildCachedTexture();
    Refresh();
  });
}

void LayoutViewerPanel::InvalidateRenderIfFrameChanged() {
  const double renderZoom = GetRenderZoom();
  for (const auto &view : currentLayout.view2dViews) {
    ViewCache &cache = GetViewCache(view.id);
    wxRect frameRect;
    if (!GetFrameRect(view.frame, frameRect)) {
      if (cache.texture != 0) {
        cache.renderDirty = true;
        renderDirty = true;
        ClearCachedTexture(cache);
        cache.textureSize = wxSize(0, 0);
        cache.renderZoom = 0.0;
      }
      continue;
    }
    const wxSize renderSize = GetFrameSizeForZoom(view.frame, renderZoom);
    if (cache.renderZoom == 0.0 || cache.renderZoom != renderZoom ||
        renderSize != cache.textureSize) {
      cache.renderDirty = true;
      renderDirty = true;
    }
  }

  for (const auto &legend : currentLayout.legendViews) {
    LegendCache &cache = GetLegendCache(legend.id);
    wxRect frameRect;
    if (!GetFrameRect(legend.frame, frameRect)) {
      if (cache.texture != 0) {
        cache.renderDirty = true;
        renderDirty = true;
        ClearCachedTexture(cache);
        cache.textureSize = wxSize(0, 0);
        cache.renderZoom = 0.0;
      }
      continue;
    }
    const wxSize renderSize = GetFrameSizeForZoom(legend.frame, renderZoom);
    if (cache.renderZoom == 0.0 || cache.renderZoom != renderZoom ||
        renderSize != cache.textureSize) {
      cache.renderDirty = true;
      renderDirty = true;
    }
  }
}

bool LayoutViewerPanel::SelectElementAtPosition(const wxPoint &pos) {
  for (int i = static_cast<int>(currentLayout.legendViews.size()) - 1; i >= 0;
       --i) {
    const auto &legend = currentLayout.legendViews[static_cast<size_t>(i)];
    wxRect frameRect;
    if (!GetFrameRect(legend.frame, frameRect))
      continue;
    if (!frameRect.Contains(pos))
      continue;
    if (selectedElementType == SelectedElementType::Legend &&
        selectedElementId == legend.id) {
      return true;
    }
    selectedElementType = SelectedElementType::Legend;
    selectedElementId = legend.id;
    RequestRenderRebuild();
    Refresh();
    return true;
  }

  for (int i = static_cast<int>(currentLayout.view2dViews.size()) - 1; i >= 0;
       --i) {
    const auto &view = currentLayout.view2dViews[static_cast<size_t>(i)];
    wxRect frameRect;
    if (!GetFrameRect(view.frame, frameRect))
      continue;
    if (!frameRect.Contains(pos))
      continue;
    if (selectedElementType == SelectedElementType::View2D &&
        selectedElementId == view.id) {
      return true;
    }
    selectedElementType = SelectedElementType::View2D;
    selectedElementId = view.id;
    RequestRenderRebuild();
    Refresh();
    return true;
  }
  return false;
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

LayoutViewerPanel::ViewCache &LayoutViewerPanel::GetViewCache(int viewId) {
  auto [it, inserted] = viewCaches_.try_emplace(viewId, ViewCache{});
  if (inserted) {
    renderDirty = true;
  }
  return it->second;
}

LayoutViewerPanel::LegendCache &LayoutViewerPanel::GetLegendCache(int legendId) {
  auto [it, inserted] = legendCaches_.try_emplace(legendId, LegendCache{});
  if (inserted) {
    renderDirty = true;
  }
  return it->second;
}

void LayoutViewerPanel::RefreshLegendData() {
  std::vector<LegendItem> items = BuildLegendItems();
  size_t newHash = HashLegendItems(items);
  if (newHash == legendDataHash)
    return;
  legendItems_ = std::move(items);
  legendDataHash = newHash;
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
      std::filesystem::path p = basePath.empty()
                                    ? std::filesystem::path(fixture.gdtfSpec)
                                    : std::filesystem::path(basePath) /
                                          fixture.gdtfSpec;
      fullPath = p.string();
    }
    if (typeName.empty() && !fullPath.empty()) {
      wxFileName fn(fullPath);
      typeName = fn.GetFullName().ToStdString();
    }
    if (typeName.empty())
      typeName = "Unknown";

    int chCount =
        GetGdtfModeChannelCount(fullPath, fixture.gdtfMode);
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
    const wxSize &size, const std::vector<LegendItem> &items,
    const SymbolDefinitionSnapshot *symbols) const {
  if (size.GetWidth() <= 0 || size.GetHeight() <= 0)
    return wxImage();
  wxBitmap bitmap(size.GetWidth(), size.GetHeight(), 32);
  wxMemoryDC dc(bitmap);
  dc.SetBackground(wxBrush(wxColour(255, 255, 255)));
  dc.Clear();
  dc.SetTextForeground(wxColour(20, 20, 20));
  dc.SetPen(*wxTRANSPARENT_PEN);

  const int padding = 8;
  const int columnGap = 8;
  const int totalRows = static_cast<int>(items.size()) + 1;
  const int availableHeight = size.GetHeight() - padding * 2;
  int fontSize =
      totalRows > 0 ? (availableHeight / totalRows) - 2 : 10;
  fontSize = std::clamp(fontSize, 6, 14);

  wxFont baseFont(fontSize, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL,
                  wxFONTWEIGHT_NORMAL);
  wxFont headerFont = baseFont;
  headerFont.SetWeight(wxFONTWEIGHT_BOLD);

  auto measureTextWidth = [&](const wxString &text) {
    int w = 0;
    int h = 0;
    dc.GetTextExtent(text, &w, &h);
    return w;
  };

  dc.SetFont(baseFont);
  int maxCountWidth = measureTextWidth("Count");
  int maxChWidth = measureTextWidth("Ch Count");
  for (const auto &item : items) {
    maxCountWidth =
        std::max(maxCountWidth, measureTextWidth(wxString::Format("%d", item.count)));
    wxString chText = item.channelCount.has_value()
                          ? wxString::Format("%d", item.channelCount.value())
                          : wxString("-");
    maxChWidth = std::max(maxChWidth, measureTextWidth(chText));
  }

  int lineHeight = 0;
  int lineWidth = 0;
  dc.GetTextExtent("Hg", &lineWidth, &lineHeight);
  lineHeight += 2;

  int symbolSize = std::max(4, lineHeight - 2);
  int xSymbol = padding;
  int xCount = xSymbol + symbolSize + columnGap;
  int xType = xCount + maxCountWidth + columnGap;
  int xCh = size.GetWidth() - padding - maxChWidth;
  if (xCh < xType + columnGap)
    xCh = xType + columnGap;
  int typeWidth = std::max(0, xCh - xType - columnGap);

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

  int y = padding;
  dc.SetFont(headerFont);
  dc.DrawText("Count", xCount, y);
  dc.DrawText("Type", xType, y);
  dc.DrawText("Ch Count", xCh, y);

  y += lineHeight;
  dc.SetPen(wxPen(wxColour(200, 200, 200)));
  dc.DrawLine(padding, y, size.GetWidth() - padding, y);
  y += 2;

  dc.SetFont(baseFont);
  LegendSymbolBackend backend(dc);
  for (const auto &item : items) {
    if (y + lineHeight > size.GetHeight() - padding)
      break;
    wxString countText = wxString::Format("%d", item.count);
    wxString typeText =
        trimTextToWidth(wxString::FromUTF8(item.typeName), typeWidth);
    wxString chText = item.channelCount.has_value()
                          ? wxString::Format("%d", item.channelCount.value())
                          : wxString("-");
    if (symbols && !item.symbolKey.empty()) {
      const SymbolDefinition *symbol =
          FindSymbolDefinition(symbols, item.symbolKey);
      if (symbol) {
        const float symbolW = symbol->bounds.max.x - symbol->bounds.min.x;
        const float symbolH = symbol->bounds.max.y - symbol->bounds.min.y;
        if (symbolW > 0.0f && symbolH > 0.0f) {
          double scale =
              std::min(static_cast<double>(symbolSize) / symbolW,
                       static_cast<double>(symbolSize) / symbolH);
          double drawW = symbolW * scale;
          double drawH = symbolH * scale;
          double symbolDrawLeft =
              xSymbol + (static_cast<double>(symbolSize) - drawW) * 0.5;
          double symbolDrawTop =
              y + (static_cast<double>(lineHeight) - drawH) * 0.5;

          viewer2d::Viewer2DRenderMapping mapping{};
          mapping.minX = symbol->bounds.min.x;
          mapping.minY = symbol->bounds.min.y;
          mapping.scale = scale;
          mapping.offsetX = symbolDrawLeft;
          mapping.offsetY = symbolDrawTop;
          mapping.drawHeight = drawH;
          viewer2d::Viewer2DCommandRenderer renderer(mapping, backend, symbols);
          renderer.Render(symbol->localCommands);
        }
      }
    }
    dc.DrawText(countText, xCount, y);
    dc.DrawText(typeText, xType, y);
    dc.DrawText(chText, xCh, y);
    y += lineHeight;
  }

  dc.SelectObject(wxNullBitmap);
  return bitmap.ConvertToImage();
}
