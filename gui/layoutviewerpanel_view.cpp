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

#include "configmanager.h"
#include "guiconfigservices.h"
#include "LayoutManager.h"
#include "ui_render_size.h"
#include "viewer2doffscreenrenderer.h"
#include "viewer2dstate.h"
#include <wx/log.h>

namespace {
// Restores on-screen GL state before drawing a cached layout texture quad.
void RestoreLayoutViewportState(LayoutViewerPanel *panel) {
  if (panel == nullptr)
    return;
  const RenderSize renderSize = ResolveRenderSize(panel);
  if (!renderSize.IsValid())
    return;
  glDisable(GL_SCISSOR_TEST);
  glViewport(0, 0, renderSize.width, renderSize.height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  const wxSize logicalSize = panel->GetClientSize();
  glOrtho(0.0, logicalSize.GetWidth(), logicalSize.GetHeight(), 0.0, -1.0,
          1.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}
}

// Returns the currently editable 2D view or falls back to the first available view.
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

// Returns the currently editable 2D view as a const pointer for read-only callers.
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

// Emits the edit request for the selected 2D layout view.
void LayoutViewerPanel::OnEditView(wxCommandEvent &) {
  if (selectedElementType != SelectedElementType::View2D)
    return;
  EmitEditViewRequest();
}

// Deletes the selected 2D layout view and removes its cached render resources.
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

// Toggles the border visibility flag for the selected 2D layout view.
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

// Applies frame geometry updates to the selected 2D layout view and persists them when needed.
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

// Captures, rebuilds, and draws the cached texture for one embedded 2D layout view.
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
  wxLogTrace(
      "layoutviewer_view_cache",
      "draw view=%d layoutZoom=%.4f frame=(%d,%d %dx%d) frameRect=(%d,%d %dx%d) "
      "cacheRenderZoom=%.4f renderSize=%dx%d textureSize=%dx%d camViewport=%dx%d "
      "capturedViewport=%dx%d fallbackViewport=%dx%d",
      view.id, zoom, view.frame.x, view.frame.y, view.frame.width,
      view.frame.height, frameRect.GetX(), frameRect.GetY(), frameRect.GetWidth(),
      frameRect.GetHeight(), cache.renderZoom, renderSize.GetWidth(),
      renderSize.GetHeight(), cache.textureSize.GetWidth(),
      cache.textureSize.GetHeight(), view.camera.viewportWidth,
      view.camera.viewportHeight, cache.viewState.viewportWidth,
      cache.viewState.viewportHeight,
      view.camera.viewportWidth > 0 ? view.camera.viewportWidth : view.frame.width,
      view.camera.viewportHeight > 0 ? view.camera.viewportHeight : view.frame.height);
  if (cache.texture != 0 && renderSize.GetWidth() > 0 &&
      renderSize.GetHeight() > 0 && cache.textureSize == renderSize) {
    RestoreLayoutViewportState(this);
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
