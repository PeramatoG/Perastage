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
#include <memory>
#include <vector>

#include <GL/gl.h>
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

#include "configmanager.h"
#include "layouts/LayoutManager.h"
#include "mainwindow.h"
#include "viewer2doffscreenrenderer.h"
#include "viewer2dstate.h"

namespace {
constexpr double kMinZoom = 0.1;
constexpr double kMaxZoom = 10.0;
constexpr double kZoomStep = 1.1;
constexpr int kZoomCacheStepsPerLevel = 2;
constexpr int kFitMarginPx = 40;
constexpr int kHandleSizePx = 10;
constexpr int kHandleHalfPx = kHandleSizePx / 2;
constexpr int kHandleHoverPadPx = 6;
constexpr int kMinFrameSize = 24;
constexpr int kMaxRenderTextureSize = 8192;
constexpr int kEditMenuId = wxID_HIGHEST + 490;
constexpr int kDeleteMenuId = wxID_HIGHEST + 491;
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
  layoutVersion++;
  captureVersion = -1;
  hasCapture = false;
  hasRenderState = false;
  cachedTextureSize = wxSize(0, 0);
  cachedRenderZoom = 0.0;
  ClearCachedTexture();
  renderDirty = true;
  ResetViewToFit();
  RequestRenderRebuild();
  Refresh();
}

void LayoutViewerPanel::OnPaint(wxPaintEvent &) {
  wxPaintDC dc(this);
  InitGL();
  SetCurrent(*glContext_);

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

  const layouts::Layout2DViewDefinition *view = GetEditableView();
  if (!view) {
    glFlush();
    SwapBuffers();
    return;
  }

  wxRect frameRect;
  if (!GetFrameRect(view->frame, frameRect)) {
    glFlush();
    SwapBuffers();
    return;
  }
  const int frameRight = frameRect.GetLeft() + frameRect.GetWidth();
  const int frameBottom = frameRect.GetTop() + frameRect.GetHeight();

  Viewer2DPanel *statePanel = nullptr;
  bool useEditorState = false;
  if (auto *mw = MainWindow::Instance()) {
    statePanel = mw->GetLayoutCapturePanel();
    useEditorState = mw->IsLayout2DViewEditing() && statePanel;
  }

  if (useEditorState && statePanel) {
    const auto editorState = statePanel->GetViewState();
    const double epsilon = 1e-4;
    bool viewChanged =
        !hasCapture || editorState.view != cachedViewState.view ||
        std::abs(editorState.zoom - cachedViewState.zoom) > epsilon ||
        std::abs(editorState.offsetPixelsX - cachedViewState.offsetPixelsX) >
            epsilon ||
        std::abs(editorState.offsetPixelsY - cachedViewState.offsetPixelsY) >
            epsilon;
    if (viewChanged)
      captureVersion = -1;
  }

  if (!captureInProgress && captureVersion != layoutVersion) {
    Viewer2DPanel *capturePanel = nullptr;
    Viewer2DOffscreenRenderer *offscreenRenderer = nullptr;
    if (auto *mw = MainWindow::Instance()) {
      offscreenRenderer = mw->GetOffscreenRenderer();
      capturePanel =
          offscreenRenderer ? offscreenRenderer->GetPanel() : nullptr;
    } else {
      capturePanel = Viewer2DPanel::Instance();
      statePanel = capturePanel;
    }
    if (capturePanel) {
      captureInProgress = true;
      const int fallbackViewportWidth =
          view->camera.viewportWidth > 0 ? view->camera.viewportWidth
                                         : view->frame.width;
      const int fallbackViewportHeight =
          view->camera.viewportHeight > 0 ? view->camera.viewportHeight
                                          : view->frame.height;
      ConfigManager &cfg = ConfigManager::Get();
      viewer2d::Viewer2DState layoutState =
          useEditorState ? viewer2d::CaptureState(statePanel, cfg)
                         : viewer2d::FromLayoutDefinition(*view);
      layoutState.renderOptions.darkMode = false;
      cachedRenderState = layoutState;
      hasRenderState = true;
      if (offscreenRenderer && fallbackViewportWidth > 0 &&
          fallbackViewportHeight > 0) {
        offscreenRenderer->SetViewportSize(
            wxSize(fallbackViewportWidth, fallbackViewportHeight));
        offscreenRenderer->PrepareForCapture();
      }
      auto stateGuard = std::make_shared<viewer2d::ScopedViewer2DState>(
          capturePanel, nullptr, cfg, layoutState);
      capturePanel->CaptureFrameNow(
          [this, stateGuard, fallbackViewportWidth, fallbackViewportHeight,
           capturePanel](
              CommandBuffer buffer, Viewer2DViewState state) {
            cachedBuffer = std::move(buffer);
            cachedViewState = state;
            if (cachedViewState.viewportWidth <= 0 &&
                fallbackViewportWidth > 0) {
              cachedViewState.viewportWidth = fallbackViewportWidth;
            }
            if (cachedViewState.viewportHeight <= 0 &&
                fallbackViewportHeight > 0) {
              cachedViewState.viewportHeight = fallbackViewportHeight;
            }
            cachedSymbols.reset();
            if (capturePanel) {
              cachedSymbols = capturePanel->GetBottomSymbolCacheSnapshot();
            }
            hasCapture = !cachedBuffer.commands.empty();
            captureVersion = layoutVersion;
            captureInProgress = false;
            renderDirty = true;
            cachedTextureSize = wxSize(0, 0);
            cachedRenderZoom = 0.0;
            RequestRenderRebuild();
            Refresh();
          });
    }
  }

  const wxSize renderSize =
      GetFrameSizeForZoom(view->frame, cachedRenderZoom);
  if (cachedTexture_ != 0 && renderSize.GetWidth() > 0 &&
      renderSize.GetHeight() > 0 &&
      cachedTextureSize == renderSize) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, cachedTexture_);
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

  glColor4ub(60, 160, 240, 255);
  glLineWidth(2.0f);
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
  double targetZoom = std::max(1.0, zoom);
  const layouts::Layout2DViewDefinition *view = GetEditableView();
  if (!view || view->frame.width <= 0 || view->frame.height <= 0)
    return targetZoom;
  const double maxZoomWidth =
      static_cast<double>(kMaxRenderTextureSize) / view->frame.width;
  const double maxZoomHeight =
      static_cast<double>(kMaxRenderTextureSize) / view->frame.height;
  const double maxZoom = std::min(maxZoomWidth, maxZoomHeight);
  if (maxZoom > 0.0)
    targetZoom = std::min(targetZoom, maxZoom);
  return targetZoom;
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

  const layouts::Layout2DViewDefinition *view = GetEditableView();
  wxRect frameRect;
  if (!view || !hasCapture || !hasRenderState ||
      !GetFrameRect(view->frame, frameRect)) {
    ClearCachedTexture();
    cachedTextureSize = wxSize(0, 0);
    cachedRenderZoom = 0.0;
    return;
  }

  Viewer2DOffscreenRenderer *offscreenRenderer = nullptr;
  Viewer2DPanel *capturePanel = nullptr;
  if (auto *mw = MainWindow::Instance()) {
    offscreenRenderer = mw->GetOffscreenRenderer();
    capturePanel = offscreenRenderer ? offscreenRenderer->GetPanel() : nullptr;
  }
  if (!capturePanel || !offscreenRenderer) {
    ClearCachedTexture();
    cachedTextureSize = wxSize(0, 0);
    cachedRenderZoom = 0.0;
    return;
  }

  const double renderZoom = GetRenderZoom();
  const wxSize renderSize = GetFrameSizeForZoom(view->frame, renderZoom);
  if (renderSize.GetWidth() <= 0 || renderSize.GetHeight() <= 0) {
    ClearCachedTexture();
    cachedTextureSize = wxSize(0, 0);
    cachedRenderZoom = 0.0;
    return;
  }

  offscreenRenderer->SetViewportSize(renderSize);
  offscreenRenderer->PrepareForCapture();

  ConfigManager &cfg = ConfigManager::Get();
  auto stateGuard = std::make_shared<viewer2d::ScopedViewer2DState>(
      capturePanel, nullptr, cfg, cachedRenderState);

  std::vector<unsigned char> pixels;
  int width = 0;
  int height = 0;
  if (!capturePanel->RenderToRGBA(pixels, width, height) || width <= 0 ||
      height <= 0) {
    ClearCachedTexture();
    cachedTextureSize = wxSize(0, 0);
    cachedRenderZoom = 0.0;
    return;
  }

  InitGL();
  SetCurrent(*glContext_);
  if (cachedTexture_ == 0) {
    glGenTextures(1, &cachedTexture_);
  }
  glBindTexture(GL_TEXTURE_2D, cachedTexture_);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, pixels.data());
  cachedTextureSize = wxSize(width, height);
  cachedRenderZoom = renderZoom;
}

void LayoutViewerPanel::ClearCachedTexture() {
  if (cachedTexture_ == 0 || !glContext_)
    return;
  SetCurrent(*glContext_);
  glDeleteTextures(1, &cachedTexture_);
  cachedTexture_ = 0;
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
  const layouts::Layout2DViewDefinition *view = GetEditableView();
  wxRect frameRect;
  if (!view || !GetFrameRect(view->frame, frameRect)) {
    if (cachedTexture_ != 0) {
      renderDirty = true;
      ClearCachedTexture();
      cachedTextureSize = wxSize(0, 0);
      cachedRenderZoom = 0.0;
    }
    return;
  }

  const double renderZoom = GetRenderZoom();
  const wxSize renderSize = GetFrameSizeForZoom(view->frame, renderZoom);
  if (cachedRenderZoom == 0.0 || cachedRenderZoom != renderZoom ||
      renderSize != cachedTextureSize)
    renderDirty = true;
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
