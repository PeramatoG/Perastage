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
constexpr int kLayoutGridStep = 5;
constexpr int kMaxRenderDimension = 8192;
constexpr size_t kMaxRenderPixels =
    static_cast<size_t>(kMaxRenderDimension) * kMaxRenderDimension;
constexpr int kEditMenuId = wxID_HIGHEST + 490;
constexpr int kDeleteMenuId = wxID_HIGHEST + 491;
constexpr int kDeleteLegendMenuId = wxID_HIGHEST + 492;
constexpr int kEditEventTableMenuId = wxID_HIGHEST + 493;
constexpr int kDeleteEventTableMenuId = wxID_HIGHEST + 494;
constexpr int kEditTextMenuId = wxID_HIGHEST + 495;
constexpr int kDeleteTextMenuId = wxID_HIGHEST + 496;
constexpr int kEditImageMenuId = wxID_HIGHEST + 497;
constexpr int kDeleteImageMenuId = wxID_HIGHEST + 498;
constexpr int kBringToFrontMenuId = wxID_HIGHEST + 499;
constexpr int kSendToBackMenuId = wxID_HIGHEST + 500;
constexpr int kLoadingTimerId = wxID_HIGHEST + 501;
constexpr int kRenderDelayTimerId = wxID_HIGHEST + 502;
constexpr int kLoadingOverlayDelayMs = 150;

int SnapToGrid(int value) {
  if (kLayoutGridStep <= 1)
    return value;
  return static_cast<int>(
      std::lround(static_cast<double>(value) / kLayoutGridStep) *
      kLayoutGridStep);
}
} // namespace

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
    EVT_KEY_DOWN(LayoutViewerPanel::OnKeyDown)
    EVT_MENU(kEditMenuId, LayoutViewerPanel::OnEditView)
    EVT_MENU(kDeleteMenuId, LayoutViewerPanel::OnDeleteView)
    EVT_MENU(kDeleteLegendMenuId, LayoutViewerPanel::OnDeleteLegend)
    EVT_MENU(kEditEventTableMenuId, LayoutViewerPanel::OnEditEventTable)
    EVT_MENU(kDeleteEventTableMenuId, LayoutViewerPanel::OnDeleteEventTable)
    EVT_MENU(kEditTextMenuId, LayoutViewerPanel::OnEditText)
    EVT_MENU(kDeleteTextMenuId, LayoutViewerPanel::OnDeleteText)
    EVT_MENU(kEditImageMenuId, LayoutViewerPanel::OnEditImage)
    EVT_MENU(kDeleteImageMenuId, LayoutViewerPanel::OnDeleteImage)
    EVT_MENU(kBringToFrontMenuId, LayoutViewerPanel::OnBringToFront)
    EVT_MENU(kSendToBackMenuId, LayoutViewerPanel::OnSendToBack)
wxEND_EVENT_TABLE()

wxDEFINE_EVENT(EVT_LAYOUT_RENDER_READY, wxCommandEvent);

LayoutViewerPanel::LayoutViewerPanel(wxWindow *parent)
    : wxGLCanvas(parent, wxID_ANY, nullptr, wxDefaultPosition,
                 wxDefaultSize,
                 wxFULL_REPAINT_ON_RESIZE | wxWANTS_CHARS) {
  SetBackgroundStyle(wxBG_STYLE_CUSTOM);
  glContext_ = new wxGLContext(this);
  currentLayout.pageSetup.pageSize = print::PageSize::A4;
  currentLayout.pageSetup.landscape = true;
  pendingFitOnResize = true;
  loadingTimer_.SetOwner(this, kLoadingTimerId);
  renderDelayTimer_.SetOwner(this, kRenderDelayTimerId);
  Bind(wxEVT_TIMER, &LayoutViewerPanel::OnLoadingTimer, this, kLoadingTimerId);
  Bind(wxEVT_TIMER, &LayoutViewerPanel::OnRenderDelayTimer, this,
       kRenderDelayTimerId);
  ResetViewToFit();
}

LayoutViewerPanel::~LayoutViewerPanel() {
  ClearCachedTexture();
  ClearLoadingTextTexture();
  loadingTimer_.Stop();
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
  } else if (!currentLayout.eventTables.empty()) {
    selectedElementType = SelectedElementType::EventTable;
    selectedElementId = currentLayout.eventTables.front().id;
  } else if (!currentLayout.textViews.empty()) {
    selectedElementType = SelectedElementType::Text;
    selectedElementId = currentLayout.textViews.front().id;
  } else if (!currentLayout.imageViews.empty()) {
    selectedElementType = SelectedElementType::Image;
    selectedElementId = currentLayout.imageViews.front().id;
  } else {
    selectedElementType = SelectedElementType::None;
    selectedElementId = -1;
  }
  layoutVersion++;
  captureInProgress = false;
  ClearCachedTexture();
  renderDirty = true;
  loadingRequested = true;
  RefreshLegendData();
  pendingFitOnResize = true;
  ResetViewToFit();
  RequestRenderRebuild();
  Refresh();
}

std::vector<LayoutViewerPanel::ZOrderedElement>
LayoutViewerPanel::BuildZOrderedElements() const {
  std::vector<ZOrderedElement> elements;
  elements.reserve(currentLayout.view2dViews.size() +
                   currentLayout.legendViews.size() +
                   currentLayout.eventTables.size() +
                   currentLayout.textViews.size() +
                   currentLayout.imageViews.size());
  size_t order = 0;
  for (const auto &view : currentLayout.view2dViews) {
    elements.push_back(
        {SelectedElementType::View2D, view.id, view.zIndex, order++});
  }
  for (const auto &legend : currentLayout.legendViews) {
    elements.push_back(
        {SelectedElementType::Legend, legend.id, legend.zIndex, order++});
  }
  for (const auto &table : currentLayout.eventTables) {
    elements.push_back(
        {SelectedElementType::EventTable, table.id, table.zIndex, order++});
  }
  for (const auto &text : currentLayout.textViews) {
    elements.push_back(
        {SelectedElementType::Text, text.id, text.zIndex, order++});
  }
  for (const auto &image : currentLayout.imageViews) {
    elements.push_back(
        {SelectedElementType::Image, image.id, image.zIndex, order++});
  }
  std::stable_sort(elements.begin(), elements.end(),
                   [](const auto &lhs, const auto &rhs) {
                     if (lhs.zIndex != rhs.zIndex)
                       return lhs.zIndex < rhs.zIndex;
                     return lhs.order < rhs.order;
                   });
  return elements;
}

std::pair<int, int> LayoutViewerPanel::GetZIndexRange() const {
  bool hasValue = false;
  int minZ = 0;
  int maxZ = 0;
  for (const auto &view : currentLayout.view2dViews) {
    if (!hasValue) {
      minZ = view.zIndex;
      maxZ = view.zIndex;
      hasValue = true;
    } else {
      minZ = std::min(minZ, view.zIndex);
      maxZ = std::max(maxZ, view.zIndex);
    }
  }
  for (const auto &legend : currentLayout.legendViews) {
    if (!hasValue) {
      minZ = legend.zIndex;
      maxZ = legend.zIndex;
      hasValue = true;
    } else {
      minZ = std::min(minZ, legend.zIndex);
      maxZ = std::max(maxZ, legend.zIndex);
    }
  }
  for (const auto &table : currentLayout.eventTables) {
    if (!hasValue) {
      minZ = table.zIndex;
      maxZ = table.zIndex;
      hasValue = true;
    } else {
      minZ = std::min(minZ, table.zIndex);
      maxZ = std::max(maxZ, table.zIndex);
    }
  }
  for (const auto &text : currentLayout.textViews) {
    if (!hasValue) {
      minZ = text.zIndex;
      maxZ = text.zIndex;
      hasValue = true;
    } else {
      minZ = std::min(minZ, text.zIndex);
      maxZ = std::max(maxZ, text.zIndex);
    }
  }
  for (const auto &image : currentLayout.imageViews) {
    if (!hasValue) {
      minZ = image.zIndex;
      maxZ = image.zIndex;
      hasValue = true;
    } else {
      minZ = std::min(minZ, image.zIndex);
      maxZ = std::max(maxZ, image.zIndex);
    }
  }
  return {minZ, maxZ};
}

void LayoutViewerPanel::OnPaint(wxPaintEvent &) {
  wxPaintDC dc(this);
  if (!IsShownOnScreen()) {
    return;
  }
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
  const int activeEventTableId =
      selectedElementType == SelectedElementType::EventTable ? selectedElementId
                                                             : -1;
  const int activeTextId =
      selectedElementType == SelectedElementType::Text ? selectedElementId
                                                       : -1;
  const int activeImageId =
      selectedElementType == SelectedElementType::Image ? selectedElementId
                                                        : -1;

  Viewer2DPanel *capturePanel = nullptr;
  Viewer2DOffscreenRenderer *offscreenRenderer = nullptr;
  if (auto *mw = MainWindow::Instance()) {
    offscreenRenderer = mw->GetOffscreenRenderer();
    capturePanel =
        offscreenRenderer ? offscreenRenderer->GetPanel() : nullptr;
  } else {
    capturePanel = Viewer2DPanel::Instance();
  }

  auto findViewById =
      [this](int viewId) -> const layouts::Layout2DViewDefinition * {
    for (const auto &view : currentLayout.view2dViews) {
      if (view.id == viewId)
        return &view;
    }
    return nullptr;
  };
  auto findLegendById =
      [this](int legendId) -> const layouts::LayoutLegendDefinition * {
    for (const auto &legend : currentLayout.legendViews) {
      if (legend.id == legendId)
        return &legend;
    }
    return nullptr;
  };
  auto findEventTableById =
      [this](int tableId) -> const layouts::LayoutEventTableDefinition * {
    for (const auto &table : currentLayout.eventTables) {
      if (table.id == tableId)
        return &table;
    }
    return nullptr;
  };
  auto findTextById =
      [this](int textId) -> const layouts::LayoutTextDefinition * {
    for (const auto &text : currentLayout.textViews) {
      if (text.id == textId)
        return &text;
    }
    return nullptr;
  };
  auto findImageById =
      [this](int imageId) -> const layouts::LayoutImageDefinition * {
    for (const auto &image : currentLayout.imageViews) {
      if (image.id == imageId)
        return &image;
    }
    return nullptr;
  };

  const auto elements = BuildZOrderedElements();
  for (const auto &element : elements) {
    if (element.type == SelectedElementType::View2D) {
      if (const auto *view = findViewById(element.id))
        DrawViewElement(*view, capturePanel, offscreenRenderer, activeViewId);
    } else if (element.type == SelectedElementType::Legend) {
      if (const auto *legend = findLegendById(element.id))
        DrawLegendElement(*legend, activeLegendId);
    } else if (element.type == SelectedElementType::EventTable) {
      if (const auto *table = findEventTableById(element.id))
        DrawEventTableElement(*table);
    } else if (element.type == SelectedElementType::Text) {
      if (const auto *text = findTextById(element.id))
        DrawTextElement(*text, activeTextId);
    } else if (element.type == SelectedElementType::Image) {
      if (const auto *image = findImageById(element.id))
        DrawImageElement(*image, activeImageId);
    }
  }

  const bool texturesReady = AreTexturesReady();
  auto isCacheUsableForFrame = [this](const auto &cache, const auto &frame) {
    if (cache.texture == 0)
      return false;
    const wxSize renderSize = GetFrameSizeForZoom(frame, cache.renderZoom);
    return renderSize.GetWidth() > 0 && renderSize.GetHeight() > 0 &&
           cache.textureSize == renderSize;
  };
  bool activeElementEmpty = false;
  if (selectedElementType == SelectedElementType::View2D) {
    if (!activeView) {
      activeElementEmpty = true;
    } else {
      auto it = viewCaches_.find(activeViewId);
      activeElementEmpty =
          it == viewCaches_.end() ||
          !isCacheUsableForFrame(it->second, activeView->frame);
    }
  } else if (selectedElementType == SelectedElementType::Legend) {
    const auto *legend = findLegendById(activeLegendId);
    if (!legend) {
      activeElementEmpty = true;
    } else {
      auto it = legendCaches_.find(activeLegendId);
      activeElementEmpty =
          it == legendCaches_.end() ||
          !isCacheUsableForFrame(it->second, legend->frame);
    }
  } else if (selectedElementType == SelectedElementType::EventTable) {
    const auto *table = findEventTableById(activeEventTableId);
    if (!table) {
      activeElementEmpty = true;
    } else {
      auto it = eventTableCaches_.find(activeEventTableId);
      activeElementEmpty =
          it == eventTableCaches_.end() ||
          !isCacheUsableForFrame(it->second, table->frame);
    }
  } else if (selectedElementType == SelectedElementType::Text) {
    const auto *text = findTextById(activeTextId);
    if (!text) {
      activeElementEmpty = true;
    } else {
      auto it = textCaches_.find(activeTextId);
      activeElementEmpty =
          it == textCaches_.end() ||
          !isCacheUsableForFrame(it->second, text->frame);
    }
  } else if (selectedElementType == SelectedElementType::Image) {
    const auto *image = findImageById(activeImageId);
    if (!image) {
      activeElementEmpty = true;
    } else {
      auto it = imageCaches_.find(activeImageId);
      activeElementEmpty =
          it == imageCaches_.end() ||
          !isCacheUsableForFrame(it->second, image->frame);
    }
  }
  const bool showLoadingOverlay = !texturesReady || activeElementEmpty;
  if (showLoadingOverlay) {
    DrawLoadingOverlay(size);
  }

  glFlush();
  SwapBuffers();
}

void LayoutViewerPanel::DrawLoadingOverlay(const wxSize &size) {
  if (size.GetWidth() <= 0 || size.GetHeight() <= 0)
    return;
  glColor4ub(0, 0, 0, 150);
  glBegin(GL_QUADS);
  glVertex2f(0.0f, 0.0f);
  glVertex2f(static_cast<float>(size.GetWidth()), 0.0f);
  glVertex2f(static_cast<float>(size.GetWidth()),
             static_cast<float>(size.GetHeight()));
  glVertex2f(0.0f, static_cast<float>(size.GetHeight()));
  glEnd();

  EnsureLoadingTextTexture();
  if (loadingTextTexture_ == 0)
    return;

  const int textWidth = loadingTextTextureSize_.GetWidth();
  const int textHeight = loadingTextTextureSize_.GetHeight();
  if (textWidth <= 0 || textHeight <= 0)
    return;

  const float x = (size.GetWidth() - textWidth) * 0.5f;
  const float y = (size.GetHeight() - textHeight) * 0.5f;

  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, loadingTextTexture_);
  glColor4ub(255, 255, 255, 255);
  glBegin(GL_QUADS);
  glTexCoord2f(0.0f, 0.0f);
  glVertex2f(x, y);
  glTexCoord2f(1.0f, 0.0f);
  glVertex2f(x + textWidth, y);
  glTexCoord2f(1.0f, 1.0f);
  glVertex2f(x + textWidth, y + textHeight);
  glTexCoord2f(0.0f, 1.0f);
  glVertex2f(x, y + textHeight);
  glEnd();
  glDisable(GL_TEXTURE_2D);
}

void LayoutViewerPanel::EnsureLoadingTextTexture() {
  if (loadingTextTexture_ != 0)
    return;

  const wxString label = wxString::FromUTF8("Loading layout...");
  wxFont font = wxFontInfo(14).Bold();
  int textWidth = 0;
  int textHeight = 0;
  {
    wxMemoryDC measureDc;
    measureDc.SetFont(font);
    measureDc.GetTextExtent(label, &textWidth, &textHeight);
  }
  if (textWidth <= 0 || textHeight <= 0)
    return;

  const int padding = 12;
  const int bmpWidth = textWidth + padding * 2;
  const int bmpHeight = textHeight + padding * 2;
  wxBitmap bitmap(bmpWidth, bmpHeight, 32);
  {
    wxMemoryDC dc(bitmap);
    dc.SetBackground(wxBrush(wxColour(0, 0, 0)));
    dc.Clear();
    dc.SetFont(font);
    dc.SetTextForeground(wxColour(255, 255, 255));
    dc.DrawText(label, padding, padding);
    dc.SelectObject(wxNullBitmap);
  }

  wxImage image = bitmap.ConvertToImage();
  if (!image.IsOk())
    return;

  const unsigned char *rgb = image.GetData();
  if (!rgb)
    return;

  std::vector<unsigned char> pixels;
  pixels.resize(static_cast<size_t>(bmpWidth) * bmpHeight * 4);
  for (int i = 0; i < bmpWidth * bmpHeight; ++i) {
    const unsigned char intensity = rgb[i * 3];
    pixels[static_cast<size_t>(i) * 4] = 255;
    pixels[static_cast<size_t>(i) * 4 + 1] = 255;
    pixels[static_cast<size_t>(i) * 4 + 2] = 255;
    pixels[static_cast<size_t>(i) * 4 + 3] = intensity;
  }

  InitGL();
  if (!IsShownOnScreen())
    return;
  SetCurrent(*glContext_);
  glGenTextures(1, &loadingTextTexture_);
  glBindTexture(GL_TEXTURE_2D, loadingTextTexture_);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bmpWidth, bmpHeight, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, pixels.data());
  loadingTextTextureSize_ = wxSize(bmpWidth, bmpHeight);
}

void LayoutViewerPanel::ClearLoadingTextTexture() {
  if (loadingTextTexture_ == 0 || !glContext_)
    return;
  if (!IsShown()) {
    loadingTextTexture_ = 0;
    loadingTextTextureSize_ = wxSize(0, 0);
    return;
  }
  SetCurrent(*glContext_);
  glDeleteTextures(1, &loadingTextTexture_);
  loadingTextTexture_ = 0;
  loadingTextTextureSize_ = wxSize(0, 0);
}

void LayoutViewerPanel::DrawSelectionHandles(const wxRect &frameRect) const {
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
}

void LayoutViewerPanel::OnSize(wxSizeEvent &) {
  wxSize size = GetClientSize();
  if (pendingFitOnResize && size.GetWidth() > 0 && size.GetHeight() > 0) {
    ResetViewToFit();
    pendingFitOnResize = false;
  } else {
    InvalidateRenderIfFrameChanged();
  }
  RequestRenderRebuild();
  Refresh();
}

void LayoutViewerPanel::OnLeftDown(wxMouseEvent &event) {
  SetFocus();
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
      if (!currentLayout.name.empty()) {
        layouts::LayoutManager::Get().BeginBatchUpdate();
      }
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
    layouts::LayoutManager::Get().EndBatchUpdate();
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
  layouts::Layout2DViewFrame selectedFrame;
  wxRect frameRect;
  if (GetSelectedFrame(selectedFrame) &&
      GetFrameRect(selectedFrame, frameRect) && frameRect.Contains(pos)) {
    if (selectedElementType == SelectedElementType::View2D) {
      EmitEditViewRequest();
      return;
    }
    if (selectedElementType == SelectedElementType::EventTable) {
      wxCommandEvent editEvent;
      OnEditEventTable(editEvent);
      return;
    }
    if (selectedElementType == SelectedElementType::Text) {
      wxCommandEvent editEvent;
      OnEditText(editEvent);
      return;
    }
    if (selectedElementType == SelectedElementType::Image) {
      wxCommandEvent editEvent;
      OnEditImage(editEvent);
      return;
    }
  }
  event.Skip();
}

void LayoutViewerPanel::OnKeyDown(wxKeyEvent &event) {
  const int key = event.GetKeyCode();
  if (key == WXK_DELETE || key == WXK_NUMPAD_DELETE) {
    wxCommandEvent deleteEvent;
    if (selectedElementType == SelectedElementType::View2D) {
      OnDeleteView(deleteEvent);
    } else if (selectedElementType == SelectedElementType::Legend) {
      OnDeleteLegend(deleteEvent);
    } else if (selectedElementType == SelectedElementType::EventTable) {
      OnDeleteEventTable(deleteEvent);
    } else if (selectedElementType == SelectedElementType::Text) {
      OnDeleteText(deleteEvent);
    } else if (selectedElementType == SelectedElementType::Image) {
      OnDeleteImage(deleteEvent);
    }
    return;
  }
  if (key == 'Z' || key == 'z') {
    ResetViewToFit();
    RequestRenderRebuild();
    Refresh();
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
      frame.x = SnapToGrid(frame.x);
      frame.y = SnapToGrid(frame.y);
    } else {
      if (selectedElementType == SelectedElementType::Image) {
        const auto *image = GetSelectedImage();
        const double ratio = image && image->aspectRatio > 0.0f
                                 ? image->aspectRatio
                                 : 0.0;
        const bool useHeight =
            dragMode == FrameDragMode::ResizeBottom ||
            (dragMode == FrameDragMode::ResizeCorner &&
             std::abs(logicalDelta.y) > std::abs(logicalDelta.x));
        if (ratio > 0.0) {
          if (dragMode == FrameDragMode::ResizeRight ||
              dragMode == FrameDragMode::ResizeCorner) {
            frame.width = std::max(
                kMinFrameSize, dragStartFrame.width + logicalDelta.x);
            frame.height = std::max(
                kMinFrameSize,
                static_cast<int>(std::lround(frame.width / ratio)));
          }
          if (dragMode == FrameDragMode::ResizeBottom ||
              dragMode == FrameDragMode::ResizeCorner) {
            const int candidateHeight = std::max(
                kMinFrameSize, dragStartFrame.height + logicalDelta.y);
            const int candidateWidth = std::max(
                kMinFrameSize,
                static_cast<int>(std::lround(candidateHeight * ratio)));
            if (dragMode == FrameDragMode::ResizeBottom ||
                std::abs(logicalDelta.y) > std::abs(logicalDelta.x)) {
              frame.height = candidateHeight;
              frame.width = candidateWidth;
            }
          }
          if (useHeight) {
            frame.height = std::max(kMinFrameSize, SnapToGrid(frame.height));
            frame.width = std::max(
                kMinFrameSize,
                static_cast<int>(std::lround(frame.height * ratio)));
          } else {
            frame.width = std::max(kMinFrameSize, SnapToGrid(frame.width));
            frame.height = std::max(
                kMinFrameSize,
                static_cast<int>(std::lround(frame.width / ratio)));
          }
        } else {
          if (dragMode == FrameDragMode::ResizeRight ||
              dragMode == FrameDragMode::ResizeCorner) {
            frame.width =
                std::max(kMinFrameSize, dragStartFrame.width + logicalDelta.x);
            frame.width = std::max(kMinFrameSize, SnapToGrid(frame.width));
          }
          if (dragMode == FrameDragMode::ResizeBottom ||
              dragMode == FrameDragMode::ResizeCorner) {
            frame.height =
                std::max(kMinFrameSize, dragStartFrame.height + logicalDelta.y);
            frame.height = std::max(kMinFrameSize, SnapToGrid(frame.height));
          }
        }
      } else {
        if (dragMode == FrameDragMode::ResizeRight ||
            dragMode == FrameDragMode::ResizeCorner) {
          frame.width =
              std::max(kMinFrameSize, dragStartFrame.width + logicalDelta.x);
          frame.width = std::max(kMinFrameSize, SnapToGrid(frame.width));
        }
        if (dragMode == FrameDragMode::ResizeBottom ||
            dragMode == FrameDragMode::ResizeCorner) {
          frame.height =
              std::max(kMinFrameSize, dragStartFrame.height + logicalDelta.y);
          frame.height = std::max(kMinFrameSize, SnapToGrid(frame.height));
        }
      }
    }
    if (selectedElementType == SelectedElementType::Legend) {
      UpdateLegendFrame(frame, dragMode == FrameDragMode::Move);
    } else if (selectedElementType == SelectedElementType::EventTable) {
      UpdateEventTableFrame(frame, dragMode == FrameDragMode::Move);
    } else if (selectedElementType == SelectedElementType::Text) {
      UpdateTextFrame(frame, dragMode == FrameDragMode::Move);
    } else if (selectedElementType == SelectedElementType::Image) {
      UpdateImageFrame(frame, dragMode == FrameDragMode::Move);
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
  if (dragMode != FrameDragMode::None) {
    layouts::LayoutManager::Get().EndBatchUpdate();
  }
  dragMode = FrameDragMode::None;
}

void LayoutViewerPanel::OnRightUp(wxMouseEvent &event) {
  SetFocus();
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
    menu.AppendSeparator();
    menu.Append(kBringToFrontMenuId, "Bring to Front");
    menu.Append(kSendToBackMenuId, "Send to Back");
  } else if (selectedElementType == SelectedElementType::Legend) {
    menu.Append(kDeleteLegendMenuId, "Delete Legend");
    menu.AppendSeparator();
    menu.Append(kBringToFrontMenuId, "Bring to Front");
    menu.Append(kSendToBackMenuId, "Send to Back");
  } else if (selectedElementType == SelectedElementType::EventTable) {
    menu.Append(kEditEventTableMenuId, "Edit Event Table");
    menu.Append(kDeleteEventTableMenuId, "Delete Event Table");
    menu.AppendSeparator();
    menu.Append(kBringToFrontMenuId, "Bring to Front");
    menu.Append(kSendToBackMenuId, "Send to Back");
  } else if (selectedElementType == SelectedElementType::Text) {
    menu.Append(kEditTextMenuId, "Edit Text");
    menu.Append(kDeleteTextMenuId, "Delete Text");
    menu.AppendSeparator();
    menu.Append(kBringToFrontMenuId, "Bring to Front");
    menu.Append(kSendToBackMenuId, "Send to Back");
  } else if (selectedElementType == SelectedElementType::Image) {
    menu.Append(kEditImageMenuId, "Change Image");
    menu.Append(kDeleteImageMenuId, "Delete Image");
    menu.AppendSeparator();
    menu.Append(kBringToFrontMenuId, "Bring to Front");
    menu.Append(kSendToBackMenuId, "Send to Back");
  }
  PopupMenu(&menu, pos);
}

void LayoutViewerPanel::OnBringToFront(wxCommandEvent &) {
  if (selectedElementId < 0)
    return;
  const int maxZ = GetZIndexRange().second;
  if (selectedElementType == SelectedElementType::View2D) {
    auto it =
        std::find_if(currentLayout.view2dViews.begin(),
                     currentLayout.view2dViews.end(),
                     [this](const auto &entry) {
                       return entry.id == selectedElementId;
                     });
    if (it == currentLayout.view2dViews.end())
      return;
    it->zIndex = maxZ + 1;
    if (!currentLayout.name.empty()) {
      layouts::LayoutManager::Get().UpdateLayout2DView(currentLayout.name, *it);
    }
  } else if (selectedElementType == SelectedElementType::Legend) {
    auto it =
        std::find_if(currentLayout.legendViews.begin(),
                     currentLayout.legendViews.end(),
                     [this](const auto &entry) {
                       return entry.id == selectedElementId;
                     });
    if (it == currentLayout.legendViews.end())
      return;
    it->zIndex = maxZ + 1;
    if (!currentLayout.name.empty()) {
      layouts::LayoutManager::Get().UpdateLayoutLegend(currentLayout.name,
                                                       *it);
    }
  } else if (selectedElementType == SelectedElementType::EventTable) {
    auto it =
        std::find_if(currentLayout.eventTables.begin(),
                     currentLayout.eventTables.end(),
                     [this](const auto &entry) {
                       return entry.id == selectedElementId;
                     });
    if (it == currentLayout.eventTables.end())
      return;
    it->zIndex = maxZ + 1;
    if (!currentLayout.name.empty()) {
      layouts::LayoutManager::Get().UpdateLayoutEventTable(currentLayout.name,
                                                           *it);
    }
  } else if (selectedElementType == SelectedElementType::Text) {
    auto it =
        std::find_if(currentLayout.textViews.begin(),
                     currentLayout.textViews.end(),
                     [this](const auto &entry) {
                       return entry.id == selectedElementId;
                     });
    if (it == currentLayout.textViews.end())
      return;
    it->zIndex = maxZ + 1;
    if (!currentLayout.name.empty()) {
      layouts::LayoutManager::Get().UpdateLayoutText(currentLayout.name, *it);
    }
  } else if (selectedElementType == SelectedElementType::Image) {
    auto it =
        std::find_if(currentLayout.imageViews.begin(),
                     currentLayout.imageViews.end(),
                     [this](const auto &entry) {
                       return entry.id == selectedElementId;
                     });
    if (it == currentLayout.imageViews.end())
      return;
    it->zIndex = maxZ + 1;
    if (!currentLayout.name.empty()) {
      layouts::LayoutManager::Get().UpdateLayoutImage(currentLayout.name, *it);
    }
  } else {
    return;
  }
  layoutVersion++;
  renderDirty = true;
  RequestRenderRebuild();
  Refresh();
}

void LayoutViewerPanel::OnSendToBack(wxCommandEvent &) {
  if (selectedElementId < 0)
    return;
  const int minZ = GetZIndexRange().first;
  if (selectedElementType == SelectedElementType::View2D) {
    auto it =
        std::find_if(currentLayout.view2dViews.begin(),
                     currentLayout.view2dViews.end(),
                     [this](const auto &entry) {
                       return entry.id == selectedElementId;
                     });
    if (it == currentLayout.view2dViews.end())
      return;
    it->zIndex = minZ - 1;
    if (!currentLayout.name.empty()) {
      layouts::LayoutManager::Get().UpdateLayout2DView(currentLayout.name, *it);
    }
  } else if (selectedElementType == SelectedElementType::Legend) {
    auto it =
        std::find_if(currentLayout.legendViews.begin(),
                     currentLayout.legendViews.end(),
                     [this](const auto &entry) {
                       return entry.id == selectedElementId;
                     });
    if (it == currentLayout.legendViews.end())
      return;
    it->zIndex = minZ - 1;
    if (!currentLayout.name.empty()) {
      layouts::LayoutManager::Get().UpdateLayoutLegend(currentLayout.name,
                                                       *it);
    }
  } else if (selectedElementType == SelectedElementType::EventTable) {
    auto it =
        std::find_if(currentLayout.eventTables.begin(),
                     currentLayout.eventTables.end(),
                     [this](const auto &entry) {
                       return entry.id == selectedElementId;
                     });
    if (it == currentLayout.eventTables.end())
      return;
    it->zIndex = minZ - 1;
    if (!currentLayout.name.empty()) {
      layouts::LayoutManager::Get().UpdateLayoutEventTable(currentLayout.name,
                                                           *it);
    }
  } else if (selectedElementType == SelectedElementType::Text) {
    auto it =
        std::find_if(currentLayout.textViews.begin(),
                     currentLayout.textViews.end(),
                     [this](const auto &entry) {
                       return entry.id == selectedElementId;
                     });
    if (it == currentLayout.textViews.end())
      return;
    it->zIndex = minZ - 1;
    if (!currentLayout.name.empty()) {
      layouts::LayoutManager::Get().UpdateLayoutText(currentLayout.name, *it);
    }
  } else if (selectedElementType == SelectedElementType::Image) {
    auto it =
        std::find_if(currentLayout.imageViews.begin(),
                     currentLayout.imageViews.end(),
                     [this](const auto &entry) {
                       return entry.id == selectedElementId;
                     });
    if (it == currentLayout.imageViews.end())
      return;
    it->zIndex = minZ - 1;
    if (!currentLayout.name.empty()) {
      layouts::LayoutManager::Get().UpdateLayoutImage(currentLayout.name, *it);
    }
  } else {
    return;
  }
  layoutVersion++;
  renderDirty = true;
  RequestRenderRebuild();
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
  const double scaledWidthValue = frame.width * targetZoom;
  const double scaledHeightValue = frame.height * targetZoom;
  if (scaledWidthValue > kMaxRenderDimension ||
      scaledHeightValue > kMaxRenderDimension)
    return wxSize(0, 0);
  const int scaledWidth = static_cast<int>(std::lround(scaledWidthValue));
  const int scaledHeight = static_cast<int>(std::lround(scaledHeightValue));
  if (scaledWidth <= 0 || scaledHeight <= 0)
    return wxSize(0, 0);
  if (scaledWidth > kMaxRenderDimension ||
      scaledHeight > kMaxRenderDimension)
    return wxSize(0, 0);
  if (static_cast<size_t>(scaledWidth) >
      kMaxRenderPixels / static_cast<size_t>(scaledHeight))
    return wxSize(0, 0);
  return wxSize(scaledWidth, scaledHeight);
}

double LayoutViewerPanel::GetRenderZoom() const {
  return zoom;
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
  if (selectedElementType == SelectedElementType::EventTable) {
    const auto *table = GetSelectedEventTable();
    if (!table)
      return false;
    frame = table->frame;
    return true;
  }
  if (selectedElementType == SelectedElementType::Text) {
    const auto *text = GetSelectedText();
    if (!text)
      return false;
    frame = text->frame;
    return true;
  }
  if (selectedElementType == SelectedElementType::Image) {
    const auto *image = GetSelectedImage();
    if (!image)
      return false;
    frame = image->frame;
    return true;
  }
  const auto *view = GetEditableView();
  if (!view)
    return false;
  frame = view->frame;
  return true;
}

void LayoutViewerPanel::InitGL() {
  if (!glContext_)
    return;
  if (!IsShownOnScreen())
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
  auto notifyRenderReady = [this]() {
    CallAfter([this]() {
      wxCommandEvent event(EVT_LAYOUT_RENDER_READY);
      event.SetEventObject(this);
      wxPostEvent(this, event);
    });
  };
  auto clearLoadingState = [this]() {
    loadingRequested = false;
    if (loadingTimer_.IsRunning())
      loadingTimer_.Stop();
    isLoading = false;
  };
  if (!IsShownOnScreen()) {
    clearLoadingState();
    notifyRenderReady();
    return;
  }

  renderDirty = false;

  Viewer2DOffscreenRenderer *offscreenRenderer = nullptr;
  Viewer2DPanel *capturePanel = nullptr;
  if (auto *mw = MainWindow::Instance()) {
    offscreenRenderer = mw->GetOffscreenRenderer();
    capturePanel = offscreenRenderer ? offscreenRenderer->GetPanel() : nullptr;
  }
  if (!capturePanel || !offscreenRenderer) {
    ClearCachedTexture();
    clearLoadingState();
    notifyRenderReady();
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
  if (legendSymbols && !legendSymbols->empty() &&
      !currentLayout.legendViews.empty()) {
    bool hasTop = false;
    bool hasFront = false;
    for (const auto &entry : *legendSymbols) {
      if (entry.second.key.viewKind == SymbolViewKind::Top)
        hasTop = true;
      else if (entry.second.key.viewKind == SymbolViewKind::Front)
        hasFront = true;
      if (hasTop && hasFront)
        break;
    }
    if (!hasTop || !hasFront) {
      const Viewer2DView previousView = capturePanel->GetView();
      auto captureMissingView = [&](Viewer2DView view) {
        capturePanel->SetView(view);
        capturePanel->CaptureFrameNow(
            [](CommandBuffer, Viewer2DViewState) {}, true, false);
      };
      if (!hasTop)
        captureMissingView(Viewer2DView::Top);
      if (!hasFront)
        captureMissingView(Viewer2DView::Front);
      capturePanel->SetView(previousView);
      legendSymbols = capturePanel->GetBottomSymbolCacheSnapshot();
    }
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
        capturePanel, nullptr, cfg, renderState, nullptr, nullptr, false);

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
    if (!IsShownOnScreen()) {
      clearLoadingState();
      notifyRenderReady();
      return;
    }
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

    wxImage image = BuildLegendImage(
        renderSize, wxSize(legend.frame.width, legend.frame.height),
        renderZoom, legendItems_, cache.symbols.get());
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
    if (!IsShownOnScreen()) {
      clearLoadingState();
      notifyRenderReady();
      return;
    }
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

  for (const auto &table : currentLayout.eventTables) {
    EventTableCache &cache = GetEventTableCache(table.id);
    size_t dataHash = HashEventTableFields(table);
    if (cache.contentHash != dataHash)
      cache.renderDirty = true;
    if (!cache.renderDirty)
      continue;
    cache.renderDirty = false;

    const wxSize renderSize = GetFrameSizeForZoom(table.frame, renderZoom);
    if (renderSize.GetWidth() <= 0 || renderSize.GetHeight() <= 0) {
      ClearCachedTexture(cache);
      cache.textureSize = wxSize(0, 0);
      cache.renderZoom = 0.0;
      continue;
    }

    wxImage image =
        BuildEventTableImage(renderSize,
                             wxSize(table.frame.width, table.frame.height),
                             renderZoom, table);
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
    const bool needsUnpremultiply = false;
    for (int i = 0; i < width * height; ++i) {
      const unsigned char a = alpha ? alpha[i] : 255;
      unsigned char r = rgb[i * 3];
      unsigned char g = rgb[i * 3 + 1];
      unsigned char b = rgb[i * 3 + 2];
      if (needsUnpremultiply && a > 0 && a < 255) {
        r = static_cast<unsigned char>(
            std::min(255, static_cast<int>(r) * 255 / a));
        g = static_cast<unsigned char>(
            std::min(255, static_cast<int>(g) * 255 / a));
        b = static_cast<unsigned char>(
            std::min(255, static_cast<int>(b) * 255 / a));
      }
      pixels[static_cast<size_t>(i) * 4] = r;
      pixels[static_cast<size_t>(i) * 4 + 1] = g;
      pixels[static_cast<size_t>(i) * 4 + 2] = b;
      pixels[static_cast<size_t>(i) * 4 + 3] = a;
    }

    InitGL();
    if (!IsShownOnScreen()) {
      clearLoadingState();
      notifyRenderReady();
      return;
    }
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
    cache.contentHash = dataHash;
  }

  for (const auto &text : currentLayout.textViews) {
    TextCache &cache = GetTextCache(text.id);
    size_t dataHash = HashTextContent(text);
    if (cache.contentHash != dataHash)
      cache.renderDirty = true;
    if (!cache.renderDirty)
      continue;
    cache.renderDirty = false;

    const wxSize renderSize = GetFrameSizeForZoom(text.frame, renderZoom);
    if (renderSize.GetWidth() <= 0 || renderSize.GetHeight() <= 0) {
      ClearCachedTexture(cache);
      cache.textureSize = wxSize(0, 0);
      cache.renderZoom = 0.0;
      continue;
    }

    wxImage image = BuildTextImage(
        renderSize, wxSize(text.frame.width, text.frame.height), renderZoom,
        text);
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
    const bool needsUnpremultiply = !text.solidBackground;
    for (int i = 0; i < width * height; ++i) {
      const unsigned char a = alpha ? alpha[i] : 255;
      unsigned char r = rgb[i * 3];
      unsigned char g = rgb[i * 3 + 1];
      unsigned char b = rgb[i * 3 + 2];
      if (needsUnpremultiply && a > 0 && a < 255) {
        r = static_cast<unsigned char>(
            std::min(255, static_cast<int>(r) * 255 / a));
        g = static_cast<unsigned char>(
            std::min(255, static_cast<int>(g) * 255 / a));
        b = static_cast<unsigned char>(
            std::min(255, static_cast<int>(b) * 255 / a));
      }
      pixels[static_cast<size_t>(i) * 4] = r;
      pixels[static_cast<size_t>(i) * 4 + 1] = g;
      pixels[static_cast<size_t>(i) * 4 + 2] = b;
      pixels[static_cast<size_t>(i) * 4 + 3] = a;
    }

    InitGL();
    if (!IsShownOnScreen()) {
      clearLoadingState();
      notifyRenderReady();
      return;
    }
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
    cache.contentHash = dataHash;
  }

  for (const auto &image : currentLayout.imageViews) {
    ImageCache &cache = GetImageCache(image.id);
    size_t dataHash = HashImageContent(image);
    if (cache.contentHash != dataHash)
      cache.renderDirty = true;
    if (!cache.renderDirty)
      continue;
    cache.renderDirty = false;

    const wxSize renderSize = GetFrameSizeForZoom(image.frame, renderZoom);
    if (renderSize.GetWidth() <= 0 || renderSize.GetHeight() <= 0) {
      ClearCachedTexture(cache);
      cache.textureSize = wxSize(0, 0);
      cache.renderZoom = 0.0;
      continue;
    }
    if (image.imagePath.empty()) {
      ClearCachedTexture(cache);
      cache.textureSize = wxSize(0, 0);
      cache.renderZoom = 0.0;
      continue;
    }

    wxImage bitmap;
    if (!bitmap.LoadFile(wxString::FromUTF8(image.imagePath))) {
      ClearCachedTexture(cache);
      cache.textureSize = wxSize(0, 0);
      cache.renderZoom = 0.0;
      continue;
    }
    if (bitmap.GetWidth() <= 0 || bitmap.GetHeight() <= 0) {
      ClearCachedTexture(cache);
      cache.textureSize = wxSize(0, 0);
      cache.renderZoom = 0.0;
      continue;
    }
    wxImage scaled =
        bitmap.Scale(renderSize.GetWidth(), renderSize.GetHeight(),
                     wxIMAGE_QUALITY_HIGH);
    if (!scaled.IsOk()) {
      ClearCachedTexture(cache);
      cache.textureSize = wxSize(0, 0);
      cache.renderZoom = 0.0;
      continue;
    }
    scaled = scaled.Mirror(false);
    if (!scaled.HasAlpha())
      scaled.InitAlpha();
    const int width = scaled.GetWidth();
    const int height = scaled.GetHeight();
    const unsigned char *rgb = scaled.GetData();
    const unsigned char *alpha = scaled.GetAlpha();
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
    if (!IsShownOnScreen()) {
      clearLoadingState();
      notifyRenderReady();
      return;
    }
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
    cache.contentHash = dataHash;
  }

  clearLoadingState();
  notifyRenderReady();
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
  for (auto &entry : eventTableCaches_) {
    ClearCachedTexture(entry.second);
  }
  eventTableCaches_.clear();
  for (auto &entry : textCaches_) {
    ClearCachedTexture(entry.second);
  }
  textCaches_.clear();
  for (auto &entry : imageCaches_) {
    ClearCachedTexture(entry.second);
  }
  imageCaches_.clear();
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

void LayoutViewerPanel::ClearCachedTexture(EventTableCache &cache) {
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

void LayoutViewerPanel::ClearCachedTexture(TextCache &cache) {
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

void LayoutViewerPanel::ClearCachedTexture(ImageCache &cache) {
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
  loadingRequested = true;
  if (!loadingTimer_.IsRunning()) {
    loadingTimer_.StartOnce(kLoadingOverlayDelayMs);
  }
  CallAfter([this]() {
    isLoading = true;
    Refresh();
    Update();
    if (wxTheApp && wxTheApp->IsMainLoopRunning())
      wxTheApp->Yield(true);
    if (renderDelayTimer_.IsRunning()) {
      renderDelayTimer_.Stop();
    }
    renderDelayTimer_.StartOnce(kLoadingOverlayDelayMs);
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

  for (const auto &table : currentLayout.eventTables) {
    EventTableCache &cache = GetEventTableCache(table.id);
    wxRect frameRect;
    if (!GetFrameRect(table.frame, frameRect)) {
      if (cache.texture != 0) {
        cache.renderDirty = true;
        renderDirty = true;
        ClearCachedTexture(cache);
        cache.textureSize = wxSize(0, 0);
        cache.renderZoom = 0.0;
      }
      continue;
    }
    const wxSize renderSize = GetFrameSizeForZoom(table.frame, renderZoom);
    if (cache.renderZoom == 0.0 || cache.renderZoom != renderZoom ||
        renderSize != cache.textureSize) {
      cache.renderDirty = true;
      renderDirty = true;
    }
  }

  for (const auto &text : currentLayout.textViews) {
    TextCache &cache = GetTextCache(text.id);
    wxRect frameRect;
    if (!GetFrameRect(text.frame, frameRect)) {
      if (cache.texture != 0) {
        cache.renderDirty = true;
        renderDirty = true;
        ClearCachedTexture(cache);
        cache.textureSize = wxSize(0, 0);
        cache.renderZoom = 0.0;
      }
      continue;
    }
    const wxSize renderSize = GetFrameSizeForZoom(text.frame, renderZoom);
    if (cache.renderZoom == 0.0 || cache.renderZoom != renderZoom ||
        renderSize != cache.textureSize) {
      cache.renderDirty = true;
      renderDirty = true;
    }
  }

  for (const auto &image : currentLayout.imageViews) {
    ImageCache &cache = GetImageCache(image.id);
    wxRect frameRect;
    if (!GetFrameRect(image.frame, frameRect)) {
      if (cache.texture != 0) {
        cache.renderDirty = true;
        renderDirty = true;
        ClearCachedTexture(cache);
        cache.textureSize = wxSize(0, 0);
        cache.renderZoom = 0.0;
      }
      continue;
    }
    const wxSize renderSize = GetFrameSizeForZoom(image.frame, renderZoom);
    if (cache.renderZoom == 0.0 || cache.renderZoom != renderZoom ||
        renderSize != cache.textureSize) {
      cache.renderDirty = true;
      renderDirty = true;
    }
  }
}

void LayoutViewerPanel::OnLoadingTimer(wxTimerEvent &) {
  if (!loadingRequested)
    return;
  if (!renderPending && !renderDirty)
    return;
  isLoading = true;
  Refresh();
}

void LayoutViewerPanel::OnRenderDelayTimer(wxTimerEvent &) {
  if (!renderPending)
    return;
  if (!renderDirty) {
    renderPending = false;
    return;
  }
  CallAfter([this]() {
    renderPending = false;
    RebuildCachedTexture();
    Refresh();
  });
}

bool LayoutViewerPanel::AreTexturesReady() const {
  auto hasTexture = [](const auto &map, int id) {
    auto it = map.find(id);
    return it != map.end() && it->second.texture != 0;
  };

  for (const auto &view : currentLayout.view2dViews) {
    if (!hasTexture(viewCaches_, view.id))
      return false;
  }
  for (const auto &legend : currentLayout.legendViews) {
    if (!hasTexture(legendCaches_, legend.id))
      return false;
  }
  for (const auto &table : currentLayout.eventTables) {
    if (!hasTexture(eventTableCaches_, table.id))
      return false;
  }
  for (const auto &text : currentLayout.textViews) {
    if (!hasTexture(textCaches_, text.id))
      return false;
  }
  for (const auto &image : currentLayout.imageViews) {
    if (!hasTexture(imageCaches_, image.id))
      return false;
  }
  return true;
}

bool LayoutViewerPanel::SelectElementAtPosition(const wxPoint &pos) {
  const auto elements = BuildZOrderedElements();
  auto findLegendById =
      [this](int legendId) -> const layouts::LayoutLegendDefinition * {
    for (const auto &legend : currentLayout.legendViews) {
      if (legend.id == legendId)
        return &legend;
    }
    return nullptr;
  };
  auto findEventTableById =
      [this](int tableId) -> const layouts::LayoutEventTableDefinition * {
    for (const auto &table : currentLayout.eventTables) {
      if (table.id == tableId)
        return &table;
    }
    return nullptr;
  };
  auto findTextById =
      [this](int textId) -> const layouts::LayoutTextDefinition * {
    for (const auto &text : currentLayout.textViews) {
      if (text.id == textId)
        return &text;
    }
    return nullptr;
  };
  auto findImageById =
      [this](int imageId) -> const layouts::LayoutImageDefinition * {
    for (const auto &image : currentLayout.imageViews) {
      if (image.id == imageId)
        return &image;
    }
    return nullptr;
  };
  auto findViewById =
      [this](int viewId) -> const layouts::Layout2DViewDefinition * {
    for (const auto &view : currentLayout.view2dViews) {
      if (view.id == viewId)
        return &view;
    }
    return nullptr;
  };
  for (auto it = elements.rbegin(); it != elements.rend(); ++it) {
    if (it->type == SelectedElementType::Legend) {
      const auto *legend = findLegendById(it->id);
      if (!legend)
        continue;
      wxRect frameRect;
      if (!GetFrameRect(legend->frame, frameRect))
        continue;
      if (!frameRect.Contains(pos))
        continue;
      if (selectedElementType == SelectedElementType::Legend &&
          selectedElementId == legend->id) {
        return true;
      }
      selectedElementType = SelectedElementType::Legend;
      selectedElementId = legend->id;
      RequestRenderRebuild();
      Refresh();
      return true;
    }

    if (it->type == SelectedElementType::EventTable) {
      const auto *table = findEventTableById(it->id);
      if (!table)
        continue;
      wxRect frameRect;
      if (!GetFrameRect(table->frame, frameRect))
        continue;
      if (!frameRect.Contains(pos))
        continue;
      if (selectedElementType == SelectedElementType::EventTable &&
          selectedElementId == table->id) {
        return true;
      }
      selectedElementType = SelectedElementType::EventTable;
      selectedElementId = table->id;
      RequestRenderRebuild();
      Refresh();
      return true;
    }

    if (it->type == SelectedElementType::Text) {
      const auto *text = findTextById(it->id);
      if (!text)
        continue;
      wxRect frameRect;
      if (!GetFrameRect(text->frame, frameRect))
        continue;
      if (!frameRect.Contains(pos))
        continue;
      if (selectedElementType == SelectedElementType::Text &&
          selectedElementId == text->id) {
        return true;
      }
      selectedElementType = SelectedElementType::Text;
      selectedElementId = text->id;
      RequestRenderRebuild();
      Refresh();
      return true;
    }

    if (it->type == SelectedElementType::Image) {
      const auto *image = findImageById(it->id);
      if (!image)
        continue;
      wxRect frameRect;
      if (!GetFrameRect(image->frame, frameRect))
        continue;
      if (!frameRect.Contains(pos))
        continue;
      if (selectedElementType == SelectedElementType::Image &&
          selectedElementId == image->id) {
        return true;
      }
      selectedElementType = SelectedElementType::Image;
      selectedElementId = image->id;
      RequestRenderRebuild();
      Refresh();
      return true;
    }

    if (it->type == SelectedElementType::View2D) {
      const auto *view = findViewById(it->id);
      if (!view)
        continue;
      wxRect frameRect;
      if (!GetFrameRect(view->frame, frameRect))
        continue;
      if (!frameRect.Contains(pos))
        continue;
      if (selectedElementType == SelectedElementType::View2D &&
          selectedElementId == view->id) {
        return true;
      }
      selectedElementType = SelectedElementType::View2D;
      selectedElementId = view->id;
      RequestRenderRebuild();
      Refresh();
      return true;
    }
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

LayoutViewerPanel::EventTableCache &
LayoutViewerPanel::GetEventTableCache(int tableId) {
  auto [it, inserted] =
      eventTableCaches_.try_emplace(tableId, EventTableCache{});
  if (inserted) {
    renderDirty = true;
  }
  return it->second;
}

LayoutViewerPanel::TextCache &LayoutViewerPanel::GetTextCache(int textId) {
  auto [it, inserted] = textCaches_.try_emplace(textId, TextCache{});
  if (inserted) {
    renderDirty = true;
  }
  return it->second;
}

LayoutViewerPanel::ImageCache &LayoutViewerPanel::GetImageCache(int imageId) {
  auto [it, inserted] = imageCaches_.try_emplace(imageId, ImageCache{});
  if (inserted) {
    renderDirty = true;
  }
  return it->second;
}
