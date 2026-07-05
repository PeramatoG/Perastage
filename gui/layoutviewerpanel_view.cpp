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
#include <memory>
#include <vector>

#include <wx/bitmap.h>
#include <wx/dcmemory.h>
#include <wx/image.h>

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

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

#include "configmanager.h"
#include "guiconfigservices.h"
#include "LayoutManager.h"
#include "viewer2doffscreenrenderer.h"
#include "viewer2dstate.h"

namespace {

constexpr const char *kLayout2DViewFailurePlaceholderText =
    "2D view render unavailable";
constexpr const char *kLayout2DViewFailurePlaceholderHint =
    "See diagnostics log";

// Draws one subtle filled rectangle for a failed interactive preview item.
void DrawFailurePlaceholderBackground(const wxRect &frameRect) {
  glColor4ub(238, 240, 242, 255);
  glBegin(GL_QUADS);
  glVertex2f(static_cast<float>(frameRect.GetLeft()),
             static_cast<float>(frameRect.GetTop()));
  glVertex2f(static_cast<float>(frameRect.GetRight()),
             static_cast<float>(frameRect.GetTop()));
  glVertex2f(static_cast<float>(frameRect.GetRight()),
             static_cast<float>(frameRect.GetBottom()));
  glVertex2f(static_cast<float>(frameRect.GetLeft()),
             static_cast<float>(frameRect.GetBottom()));
  glEnd();

  glColor4ub(185, 190, 196, 255);
  glLineWidth(1.0f);
  glBegin(GL_LINE_LOOP);
  glVertex2f(static_cast<float>(frameRect.GetLeft()),
             static_cast<float>(frameRect.GetTop()));
  glVertex2f(static_cast<float>(frameRect.GetRight()),
             static_cast<float>(frameRect.GetTop()));
  glVertex2f(static_cast<float>(frameRect.GetRight()),
             static_cast<float>(frameRect.GetBottom()));
  glVertex2f(static_cast<float>(frameRect.GetLeft()),
             static_cast<float>(frameRect.GetBottom()));
  glEnd();
}

// Builds an RGBA texture containing the preview failure message.
unsigned int CreateFailurePlaceholderTextTexture(const wxRect &frameRect,
                                                 wxSize &textureSize) {
  const bool showHint = frameRect.GetWidth() >= 180 && frameRect.GetHeight() >= 70;
  wxFont titleFont = wxFontInfo(10).Bold();
  wxFont hintFont = wxFontInfo(9);
  int titleWidth = 0;
  int titleHeight = 0;
  int hintWidth = 0;
  int hintHeight = 0;
  {
    wxMemoryDC measureDc;
    measureDc.SetFont(titleFont);
    measureDc.GetTextExtent(kLayout2DViewFailurePlaceholderText, &titleWidth,
                            &titleHeight);
    if (showHint) {
      measureDc.SetFont(hintFont);
      measureDc.GetTextExtent(kLayout2DViewFailurePlaceholderHint, &hintWidth,
                              &hintHeight);
    }
  }
  const int padding = 8;
  const int bmpWidth =
      std::max(titleWidth, showHint ? hintWidth : 0) + padding * 2;
  const int bmpHeight =
      titleHeight + (showHint ? hintHeight + 4 : 0) + padding * 2;
  if (bmpWidth <= 0 || bmpHeight <= 0)
    return 0;

  wxBitmap bitmap(bmpWidth, bmpHeight, 32);
  {
    wxMemoryDC dc(bitmap);
    dc.SetBackground(wxBrush(wxColour(238, 240, 242)));
    dc.Clear();
    dc.SetFont(titleFont);
    dc.SetTextForeground(wxColour(88, 94, 102));
    dc.DrawText(kLayout2DViewFailurePlaceholderText, padding, padding);
    if (showHint) {
      dc.SetFont(hintFont);
      dc.SetTextForeground(wxColour(112, 118, 126));
      dc.DrawText(kLayout2DViewFailurePlaceholderHint, padding,
                  padding + titleHeight + 4);
    }
    dc.SelectObject(wxNullBitmap);
  }

  wxImage image = bitmap.ConvertToImage();
  if (!image.IsOk() || !image.GetData())
    return 0;
  std::vector<unsigned char> pixels(static_cast<size_t>(bmpWidth) * bmpHeight *
                                    4);
  const unsigned char *rgb = image.GetData();
  for (int i = 0; i < bmpWidth * bmpHeight; ++i) {
    pixels[static_cast<size_t>(i) * 4] = rgb[i * 3];
    pixels[static_cast<size_t>(i) * 4 + 1] = rgb[i * 3 + 1];
    pixels[static_cast<size_t>(i) * 4 + 2] = rgb[i * 3 + 2];
    pixels[static_cast<size_t>(i) * 4 + 3] = 255;
  }

  unsigned int texture = 0;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bmpWidth, bmpHeight, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, pixels.data());
  textureSize = wxSize(bmpWidth, bmpHeight);
  return texture;
}

// Draws the preview-only failure placeholder text inside the layout view frame.
void DrawFailurePlaceholderText(const wxRect &frameRect) {
  wxSize textureSize;
  const unsigned int texture =
      CreateFailurePlaceholderTextTexture(frameRect, textureSize);
  if (texture == 0)
    return;

  const int left = frameRect.GetLeft() +
                   (frameRect.GetWidth() - textureSize.GetWidth()) / 2;
  const int top = frameRect.GetTop() +
                  (frameRect.GetHeight() - textureSize.GetHeight()) / 2;
  const int right = left + textureSize.GetWidth();
  const int bottom = top + textureSize.GetHeight();
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, texture);
  glColor4ub(255, 255, 255, 255);
  glBegin(GL_QUADS);
  glTexCoord2f(0.0f, 1.0f);
  glVertex2f(static_cast<float>(left), static_cast<float>(top));
  glTexCoord2f(1.0f, 1.0f);
  glVertex2f(static_cast<float>(right), static_cast<float>(top));
  glTexCoord2f(1.0f, 0.0f);
  glVertex2f(static_cast<float>(right), static_cast<float>(bottom));
  glTexCoord2f(0.0f, 0.0f);
  glVertex2f(static_cast<float>(left), static_cast<float>(bottom));
  glEnd();
  glDisable(GL_TEXTURE_2D);
  glDeleteTextures(1, &texture);
}

// Draws the non-printing placeholder for a failed Layout 2D preview item.
void DrawFailurePlaceholder(const wxRect &frameRect) {
  DrawFailurePlaceholderBackground(frameRect);
  if (frameRect.GetWidth() >= 120 && frameRect.GetHeight() >= 36)
    DrawFailurePlaceholderText(frameRect);
}

} // namespace

// Returns the currently editable Layout 2D view.
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

// Returns the currently editable Layout 2D view without mutating state.
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

// Marks one 2D view cache dirty and schedules a rebuild only for the updated view element.
void LayoutViewerPanel::RefreshEditedViewById(int viewId) {
  if (viewId <= 0)
    return;
  auto cacheIt = viewCaches_.find(viewId);
  if (cacheIt != viewCaches_.end()) {
    cacheIt->second.captureVersion = -1;
    cacheIt->second.captureInProgress = false;
    cacheIt->second.hasCapture = false;
    cacheIt->second.hasRenderState = false;
    cacheIt->second.hasCaptureContentHash = false;
    cacheIt->second.restoredFromPersistentCache = false;
    cacheIt->second.renderDirty = true;
    cacheIt->second.contentHash = 0;
  } else {
    ViewCache &cache = GetViewCache(viewId);
    cache.captureVersion = -1;
    cache.captureInProgress = false;
    cache.hasCapture = false;
    cache.hasRenderState = false;
    cache.hasCaptureContentHash = false;
    cache.restoredFromPersistentCache = false;
    cache.renderDirty = true;
    cache.contentHash = 0;
  }
  renderDirty = true;
  RequestRenderRebuild();
  Refresh();
}

// Emits a request to edit the selected Layout 2D view.
void LayoutViewerPanel::OnEditView(wxCommandEvent &) {
  if (selectedElementType != SelectedElementType::View2D)
    return;
  EmitEditViewRequest();
}

// Deletes the selected Layout 2D view from the current layout.
void LayoutViewerPanel::OnDeleteView(wxCommandEvent &) {
  if (selectedElementType != SelectedElementType::View2D)
    return;
  const layouts::Layout2DViewDefinition *view = GetEditableView();
  if (!view)
    return;
  const int viewId = view->id;
  if (!currentLayout.name.empty()) {
    auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
    cfg.PushUndoState("delete layout 2d view");
    if (layouts::LayoutManager::Get().RemoveLayout2DView(currentLayout.name,
                                                        viewId)) {
      auto &views = currentLayout.view2dViews;
      views.erase(std::remove_if(views.begin(), views.end(),
                                 [viewId](const auto &entry) {
                                   return entry.id == viewId;
                                 }),
                  views.end());
      InvalidateSelectionIndexCache();
      if (selectedElementType == SelectedElementType::View2D &&
          selectedElementId == viewId) {
        if (!views.empty()) {
          selectedElementType = SelectedElementType::View2D;
          selectedElementId = views.front().id;
        } else if (!currentLayout.legendViews.empty()) {
          selectedElementType = SelectedElementType::Legend;
          selectedElementId = currentLayout.legendViews.front().id;
        } else if (!currentLayout.imageViews.empty()) {
          selectedElementType = SelectedElementType::Image;
          selectedElementId = currentLayout.imageViews.front().id;
        } else if (!currentLayout.textViews.empty()) {
          selectedElementType = SelectedElementType::Text;
          selectedElementId = currentLayout.textViews.front().id;
        } else if (!currentLayout.eventTables.empty()) {
          selectedElementType = SelectedElementType::EventTable;
          selectedElementId = currentLayout.eventTables.front().id;
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

// Toggles the visible frame for the selected Layout 2D view.
void LayoutViewerPanel::OnToggleViewFrame(wxCommandEvent &) {
  if (selectedElementType != SelectedElementType::View2D)
    return;
  layouts::Layout2DViewDefinition *view = GetEditableView();
  if (!view)
    return;
  view->drawFrame = !view->drawFrame;
  if (!currentLayout.name.empty()) {
    auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
    cfg.PushUndoState("toggle layout 2d view border");
    layouts::LayoutManager::Get().UpdateLayout2DView(currentLayout.name,
                                                     *view);
  }
  RequestRenderRebuild();
  Refresh();
}

// Updates the selected Layout 2D view frame and invalidates cached rendering.
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
  if (updatePosition) {
    pendingFrameCommit_ = true;
    Refresh();
    return;
  }
  if (!currentLayout.name.empty()) {
    layouts::LayoutManager::Get().UpdateLayout2DView(currentLayout.name,
                                                     *view);
  }
  InvalidateRenderIfFrameChanged(false);
  if (NeedsRenderRebuild()) {
    RequestRenderRebuild();
  }
  Refresh();
}

// Draws one interactive Layout 2D view preview element.
void LayoutViewerPanel::DrawViewElement(
    const layouts::Layout2DViewDefinition &view, Viewer2DPanel *capturePanel,
    Viewer2DOffscreenRenderer *offscreenRenderer, int activeViewId) {
  ViewCache &cache = GetViewCache(view.id);
  const size_t viewContentHash = HashViewContent(view);
  const bool needsCapture =
      !cache.hasCapture || !cache.hasRenderState ||
      cache.captureVersion != viewRenderVersion ||
      !cache.hasCaptureContentHash || cache.captureContentHash != viewContentHash;
  if (!captureInProgress && !cache.captureInProgress && needsCapture &&
      capturePanel) {
    captureInProgress = true;
    cache.captureInProgress = true;
    const int viewId = view.id;
    const size_t captureContentHash = viewContentHash;
    const int fallbackViewportWidth = view.camera.viewportWidth > 0
                                          ? view.camera.viewportWidth
                                          : view.frame.width;
    const int fallbackViewportHeight = view.camera.viewportHeight > 0
                                           ? view.camera.viewportHeight
                                           : view.frame.height;
    ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
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
        capturePanel, nullptr, cfg, layoutState, nullptr, nullptr, false);
    capturePanel->CaptureFrameNow(
        [this, viewId, stateGuard, fallbackViewportWidth, fallbackViewportHeight,
         capturePanel, captureContentHash](CommandBuffer buffer,
                                           Viewer2DViewState state) {
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
            cache.symbols = capturePanel->GetBottomSymbolCacheSnapshot();
          }
          cache.hasCapture = !cache.buffer.commands.empty();
          cache.captureContentHash = captureContentHash;
          cache.hasCaptureContentHash = true;
          cache.captureVersion = viewRenderVersion;
          cache.restoredFromPersistentCache = false;
          cache.captureInProgress = false;
          captureInProgress = false;
          cache.renderDirty = true;
          renderDirty = true;
          cache.textureSize = wxSize(0, 0);
          cache.renderZoom = 0.0;
          RequestRenderRebuild();
          Refresh();
        },
        true, true);
  }

  wxRect frameRect;
  if (!GetFrameRect(view.frame, frameRect))
    return;
  const int frameRight = frameRect.GetLeft() + frameRect.GetWidth();
  const int frameBottom = frameRect.GetTop() + frameRect.GetHeight();

  const wxSize renderSize = GetFrameSizeForZoom(view.frame, cache.renderZoom);
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
  } else if (cache.hasLastRenderFailure) {
    DrawFailurePlaceholder(frameRect);
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

  if (view.drawFrame || view.id == activeViewId) {
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
  }

  if (view.id == activeViewId)
    DrawSelectionHandles(frameRect);
}
