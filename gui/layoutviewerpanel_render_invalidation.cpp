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

#include <functional>

#include "guiconfigservices.h"

namespace {
void HashCombine(size_t &seed, size_t value) {
  seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

void HashCombineFloat(size_t &seed, float value) {
  HashCombine(seed, std::hash<float>{}(value));
}

template <typename TMap>
size_t HashSceneUuidMap(const TMap &items) {
  size_t aggregate = std::hash<size_t>{}(items.size());
  const std::hash<std::string> strHasher;
  for (const auto &[uuid, value] : items) {
    (void)value;
    const size_t entryHash = strHasher(uuid);
    aggregate ^= entryHash + 0x9e3779b9 + (aggregate << 6) + (aggregate >> 2);
  }
  return aggregate;
}
} // namespace

size_t LayoutViewerPanel::ComputeSceneContentHash() const {
  const auto &scene = GetDefaultGuiConfigServices().LegacyConfigManager().GetScene();
  size_t hash = 0;
  HashCombine(hash, HashSceneUuidMap(scene.fixtures));
  HashCombine(hash, HashSceneUuidMap(scene.trusses));
  HashCombine(hash, HashSceneUuidMap(scene.sceneObjects));
  HashCombine(hash, HashSceneUuidMap(scene.supports));
  return hash;
}

size_t LayoutViewerPanel::HashViewContent(
    const layouts::Layout2DViewDefinition &view) const {
  size_t seed = std::hash<int>{}(view.id);
  HashCombine(seed, std::hash<float>{}(view.camera.offsetPixelsX));
  HashCombine(seed, std::hash<float>{}(view.camera.offsetPixelsY));
  HashCombine(seed, std::hash<float>{}(view.camera.zoom));
  HashCombine(seed, std::hash<int>{}(view.camera.view));
  HashCombine(seed, std::hash<int>{}(view.renderOptions.renderMode));
  HashCombine(seed, std::hash<bool>{}(view.renderOptions.darkMode));
  if (view.renderOptions.forceBottomViewForTopFixtures.has_value()) {
    HashCombine(seed, std::hash<int>{}(1));
    HashCombine(seed, std::hash<bool>{}(
                          view.renderOptions.forceBottomViewForTopFixtures.value()));
  } else {
    HashCombine(seed, std::hash<int>{}(0));
  }
  HashCombine(seed, std::hash<bool>{}(view.renderOptions.showGrid));
  HashCombine(seed, std::hash<int>{}(view.renderOptions.gridStyle));
  HashCombineFloat(seed, view.renderOptions.gridColorR);
  HashCombineFloat(seed, view.renderOptions.gridColorG);
  HashCombineFloat(seed, view.renderOptions.gridColorB);
  HashCombine(seed, std::hash<bool>{}(view.renderOptions.gridDrawAbove));
  HashCombine(seed, std::hash<bool>{}(view.renderOptions.showRuler));
  for (float red : view.renderOptions.rulerColorR)
    HashCombineFloat(seed, red);
  for (float green : view.renderOptions.rulerColorG)
    HashCombineFloat(seed, green);
  for (float blue : view.renderOptions.rulerColorB)
    HashCombineFloat(seed, blue);

  for (bool enabled : view.renderOptions.showLabelName)
    HashCombine(seed, std::hash<bool>{}(enabled));
  for (bool enabled : view.renderOptions.showLabelId)
    HashCombine(seed, std::hash<bool>{}(enabled));
  for (bool enabled : view.renderOptions.showLabelDmx)
    HashCombine(seed, std::hash<bool>{}(enabled));
  HashCombineFloat(seed, view.renderOptions.labelFontSizeName);
  HashCombineFloat(seed, view.renderOptions.labelFontSizeId);
  HashCombineFloat(seed, view.renderOptions.labelFontSizeDmx);
  for (float distance : view.renderOptions.labelOffsetDistance)
    HashCombineFloat(seed, distance);
  for (float angle : view.renderOptions.labelOffsetAngle)
    HashCombineFloat(seed, angle);

  for (const auto &hiddenLayer : view.layers.hiddenLayers)
    HashCombine(seed, std::hash<std::string>{}(hiddenLayer));
  for (const auto &hiddenFixtureType : view.layers.hiddenFixtureTypes)
    HashCombine(seed, std::hash<std::string>{}(hiddenFixtureType));
  return seed;
}

void LayoutViewerPanel::InvalidateRenderIfFrameChanged() {
  const double renderZoom = GetRenderZoom();
  const double pageWidth = currentLayout.pageSetup.PageWidthPt();
  const double pageHeight = currentLayout.pageSetup.PageHeightPt();
  const bool zoomChanged = lastRenderZoom != renderZoom;
  const bool pageChanged =
      lastPageWidthPt != pageWidth || lastPageHeightPt != pageHeight;
  const size_t sceneContentHash = ComputeSceneContentHash();
  const bool sceneContentChanged =
      !hasSceneContentHash || sceneContentHash != lastSceneContentHash;
  auto markDirty = [&](bool &cacheDirty) {
    if (cacheDirty)
      return;
    cacheDirty = true;
  };

  for (const auto &view : currentLayout.view2dViews) {
    ViewCache &cache = GetViewCache(view.id);
    const size_t contentHash = HashViewContent(view);
    if (cache.contentHash != 0 && cache.contentHash != contentHash) {
      markDirty(cache.renderDirty);
    }
    if (sceneContentChanged) {
      cache.captureVersion = -1;
      cache.captureInProgress = false;
      markDirty(cache.renderDirty);
    }
    wxRect frameRect;
    if (!GetFrameRect(view.frame, frameRect)) {
      if (cache.texture != 0) {
        markDirty(cache.renderDirty);
        ClearCachedTexture(cache);
        cache.textureSize = wxSize(0, 0);
        cache.renderZoom = 0.0;
      }
      continue;
    }
    const wxSize renderSize = GetFrameSizeForZoom(view.frame, renderZoom);
    if (cache.renderZoom == 0.0 || cache.renderZoom != renderZoom ||
        renderSize != cache.textureSize) {
      markDirty(cache.renderDirty);
    }
  }

  for (const auto &legend : currentLayout.legendViews) {
    LegendCache &cache = GetLegendCache(legend.id);
    if (sceneContentChanged) {
      markDirty(cache.renderDirty);
    }
    wxRect frameRect;
    if (!GetFrameRect(legend.frame, frameRect)) {
      if (cache.texture != 0) {
        markDirty(cache.renderDirty);
        ClearCachedTexture(cache);
        cache.textureSize = wxSize(0, 0);
        cache.renderZoom = 0.0;
      }
      continue;
    }
    const wxSize renderSize = GetFrameSizeForZoom(legend.frame, renderZoom);
    if (cache.renderZoom == 0.0 || cache.renderZoom != renderZoom ||
        renderSize != cache.textureSize) {
      markDirty(cache.renderDirty);
    }
  }

  for (const auto &table : currentLayout.eventTables) {
    EventTableCache &cache = GetEventTableCache(table.id);
    wxRect frameRect;
    if (!GetFrameRect(table.frame, frameRect)) {
      if (cache.texture != 0) {
        markDirty(cache.renderDirty);
        ClearCachedTexture(cache);
        cache.textureSize = wxSize(0, 0);
        cache.renderZoom = 0.0;
      }
      continue;
    }
    const wxSize renderSize = GetFrameSizeForZoom(table.frame, renderZoom);
    if (cache.renderZoom == 0.0 || cache.renderZoom != renderZoom ||
        renderSize != cache.textureSize) {
      markDirty(cache.renderDirty);
    }
  }

  for (const auto &text : currentLayout.textViews) {
    TextCache &cache = GetTextCache(text.id);
    wxRect frameRect;
    if (!GetFrameRect(text.frame, frameRect)) {
      if (cache.texture != 0) {
        markDirty(cache.renderDirty);
        ClearCachedTexture(cache);
        cache.textureSize = wxSize(0, 0);
        cache.renderZoom = 0.0;
      }
      continue;
    }
    const wxSize renderSize = GetFrameSizeForZoom(text.frame, renderZoom);
    if (cache.renderZoom == 0.0 || cache.renderZoom != renderZoom ||
        renderSize != cache.textureSize) {
      markDirty(cache.renderDirty);
    }
  }

  for (const auto &image : currentLayout.imageViews) {
    ImageCache &cache = GetImageCache(image.id);
    wxRect frameRect;
    if (!GetFrameRect(image.frame, frameRect)) {
      if (cache.texture != 0) {
        markDirty(cache.renderDirty);
        ClearCachedTexture(cache);
        cache.textureSize = wxSize(0, 0);
        cache.renderZoom = 0.0;
      }
      continue;
    }
    const wxSize renderSize = GetFrameSizeForZoom(image.frame, renderZoom);
    if (cache.renderZoom == 0.0 || cache.renderZoom != renderZoom ||
        renderSize != cache.textureSize) {
      markDirty(cache.renderDirty);
    }
  }

  if (zoomChanged || pageChanged || sceneContentChanged) {
    renderDirty = true;
  }
  lastRenderZoom = renderZoom;
  lastPageWidthPt = pageWidth;
  lastPageHeightPt = pageHeight;
  lastSceneContentHash = sceneContentHash;
  hasSceneContentHash = true;
}

void LayoutViewerPanel::RefreshAfterSceneContentUpdate() {
  viewRenderVersion++;
  captureInProgress = false;

  for (auto &entry : viewCaches_) {
    ViewCache &cache = entry.second;
    cache.captureVersion = -1;
    cache.captureInProgress = false;
    cache.renderDirty = true;
  }

  for (auto &entry : legendCaches_)
    entry.second.renderDirty = true;

  renderDirty = true;
  RequestRenderRebuild();
  Refresh();
}

void LayoutViewerPanel::RefreshAfterFixtureSymbolUpdate() {
  RefreshAfterSceneContentUpdate();
}
