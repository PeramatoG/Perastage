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

#include <GL/glew.h>
#include <GL/gl.h>

#include "configmanager.h"
#include "layouts/LayoutManager.h"
#include "viewer2dstate.h"

namespace {
void HashCombine(size_t &seed, size_t value) {
  seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

template <typename T> void HashCombineValue(size_t &seed, const T &value) {
  HashCombine(seed, std::hash<T>{}(value));
}

size_t HashLayoutViewRenderState(const layouts::Layout2DViewDefinition &view) {
  size_t seed = 0;
  const auto &camera = view.camera;
  HashCombineValue(seed, camera.offsetPixelsX);
  HashCombineValue(seed, camera.offsetPixelsY);
  HashCombineValue(seed, camera.zoom);
  HashCombineValue(seed, camera.viewportWidth);
  HashCombineValue(seed, camera.viewportHeight);
  HashCombineValue(seed, camera.view);

  const auto &options = view.renderOptions;
  HashCombineValue(seed, options.renderMode);
  HashCombineValue(seed, options.darkMode);
  HashCombineValue(seed, options.showGrid);
  HashCombineValue(seed, options.gridStyle);
  HashCombineValue(seed, options.gridColorR);
  HashCombineValue(seed, options.gridColorG);
  HashCombineValue(seed, options.gridColorB);
  HashCombineValue(seed, options.gridDrawAbove);

  for (bool value : options.showLabelName)
    HashCombineValue(seed, value);
  for (bool value : options.showLabelId)
    HashCombineValue(seed, value);
  for (bool value : options.showLabelDmx)
    HashCombineValue(seed, value);
  HashCombineValue(seed, options.labelFontSizeName);
  HashCombineValue(seed, options.labelFontSizeId);
  HashCombineValue(seed, options.labelFontSizeDmx);
  for (float value : options.labelOffsetDistance)
    HashCombineValue(seed, value);
  for (float value : options.labelOffsetAngle)
    HashCombineValue(seed, value);

  HashCombineValue(seed, view.layers.hiddenLayers.size());
  for (const auto &layer : view.layers.hiddenLayers)
    HashCombineValue(seed, layer);
  return seed;
}
} // namespace

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

bool LayoutViewerPanel::UpdateViewCacheState(
    const layouts::Layout2DViewDefinition &view, ViewCache &cache) {
  const size_t nextHash = HashLayoutViewRenderState(view);
  if (cache.hasRenderState && cache.renderStateHash == nextHash) {
    return false;
  }
  viewer2d::Viewer2DState layoutState = viewer2d::FromLayoutDefinition(view);
  layoutState.renderOptions.darkMode = false;
  cache.renderState = layoutState;
  cache.hasRenderState = true;
  cache.renderStateHash = nextHash;
  cache.renderDirty = true;
  renderDirty = true;
  cache.captureVersion = -1;
  if (cache.captureInProgress)
    cache.captureDirty = true;
  return true;
}

void LayoutViewerPanel::DrawViewElement(
    const layouts::Layout2DViewDefinition &view, Viewer2DPanel *capturePanel,
    int activeViewId) {
  ViewCache &cache = GetViewCache(view.id);
  if (UpdateViewCacheState(view, cache) ||
      cache.captureVersion != layoutVersion) {
    if (!cache.captureInProgress)
      RequestRenderRebuild();
  }
  if (cache.captureVersion != layoutVersion) {
    RequestRenderRebuild();
  }
  if (cache.captureInProgress) {
    if (cache.captureVersion != layoutVersion)
      cache.captureDirty = true;
  } else if (cache.captureVersion != layoutVersion && capturePanel) {
    cache.captureInProgress = true;
    cache.captureDirty = false;
    const int viewId = view.id;
    const int captureVersion = layoutVersion;
    const int fallbackViewportWidth = view.camera.viewportWidth > 0
                                          ? view.camera.viewportWidth
                                          : view.frame.width;
    const int fallbackViewportHeight = view.camera.viewportHeight > 0
                                           ? view.camera.viewportHeight
                                           : view.frame.height;
    ConfigManager &cfg = ConfigManager::Get();
    auto stateGuard = std::make_shared<viewer2d::ScopedViewer2DState>(
        capturePanel, nullptr, cfg, cache.renderState, nullptr, nullptr,
        false);
    capturePanel->CaptureFrameAsync(
        [this, viewId, captureVersion, stateGuard, fallbackViewportWidth,
         fallbackViewportHeight,
         capturePanel](CommandBuffer buffer, Viewer2DViewState state) {
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
          cache.hasCapture = true;
          cache.captureVersion = captureVersion;
          cache.captureInProgress = false;
          cache.renderDirty = true;
          renderDirty = true;
          if (cache.captureDirty || captureVersion != layoutVersion) {
            cache.captureDirty = false;
            cache.captureVersion = -1;
          }
          RequestRenderRebuild();
          Refresh();
        });
  }

  wxRect frameRect;
  if (!GetFrameRect(view.frame, frameRect))
    return;
  const int frameRight = frameRect.GetLeft() + frameRect.GetWidth();
  const int frameBottom = frameRect.GetTop() + frameRect.GetHeight();

  const wxSize renderSize = GetFrameSizeForZoom(view.frame, cache.renderZoom);
  const bool hasCachedTexture =
      cache.texture != 0 &&
      (cache.renderDirty ||
       (renderSize.GetWidth() > 0 && renderSize.GetHeight() > 0 &&
        cache.textureSize == renderSize));
  if (hasCachedTexture) {
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
    if (cache.texture == 0) {
      DrawLoadingOverlay(frameRect);
    }
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
    DrawSelectionHandles(frameRect);
}
