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

#include "configmanager.h"
#include "guiconfigservices.h"

namespace {
// Mixes a value into an aggregate hash seed.
void HashCombine(size_t &seed, size_t value) {
  seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

// Mixes a floating-point value into an aggregate hash seed.
void HashCombineFloat(size_t &seed, float value) {
  HashCombine(seed, std::hash<float>{}(value));
}

// Hashes a transform matrix for scene-content change detection.
size_t HashMatrixValue(const Matrix &matrix) {
  size_t hash = 0;
  HashCombineFloat(hash, matrix.u[0]);
  HashCombineFloat(hash, matrix.u[1]);
  HashCombineFloat(hash, matrix.u[2]);
  HashCombineFloat(hash, matrix.v[0]);
  HashCombineFloat(hash, matrix.v[1]);
  HashCombineFloat(hash, matrix.v[2]);
  HashCombineFloat(hash, matrix.w[0]);
  HashCombineFloat(hash, matrix.w[1]);
  HashCombineFloat(hash, matrix.w[2]);
  HashCombineFloat(hash, matrix.o[0]);
  HashCombineFloat(hash, matrix.o[1]);
  HashCombineFloat(hash, matrix.o[2]);
  return hash;
}

// Hashes a scene object and its nested geometry data.
size_t HashSceneObjectValue(const SceneObject &object) {
  size_t hash = std::hash<std::string>{}(object.layer);
  HashCombine(hash, std::hash<std::string>{}(object.name));
  HashCombine(hash, std::hash<std::string>{}(object.modelFile));
  HashCombine(hash, HashMatrixValue(object.transform));
  HashCombine(hash, std::hash<size_t>{}(object.geometries.size()));
  for (const auto &geometry : object.geometries) {
    HashCombine(hash, std::hash<std::string>{}(geometry.modelFile));
    HashCombine(hash, HashMatrixValue(geometry.localTransform));
  }
  return hash;
}

// Hashes fixture fields that affect layout rendering output.
size_t HashFixtureValue(const Fixture &fixture) {
  size_t hash = std::hash<std::string>{}(fixture.layer);
  HashCombine(hash, std::hash<std::string>{}(fixture.instanceName));
  HashCombine(hash, std::hash<std::string>{}(fixture.typeName));
  HashCombine(hash, std::hash<std::string>{}(fixture.gdtfSpec));
  HashCombine(hash, std::hash<std::string>{}(fixture.gdtfMode));
  HashCombine(hash, std::hash<std::string>{}(fixture.color));
  HashCombine(hash, HashMatrixValue(fixture.transform));
  return hash;
}

// Hashes truss fields that affect layout rendering output.
size_t HashTrussValue(const Truss &truss) {
  size_t hash = std::hash<std::string>{}(truss.layer);
  HashCombine(hash, std::hash<std::string>{}(truss.name));
  HashCombine(hash, std::hash<std::string>{}(truss.symbolFile));
  HashCombine(hash, std::hash<std::string>{}(truss.modelFile));
  HashCombine(hash, HashMatrixValue(truss.transform));
  return hash;
}

// Hashes support fields that affect layout rendering output.
size_t HashSupportValue(const Support &support) {
  size_t hash = std::hash<std::string>{}(support.layer);
  HashCombine(hash, std::hash<std::string>{}(support.name));
  HashCombine(hash, std::hash<std::string>{}(support.hoistFunction));
  HashCombine(hash, HashMatrixValue(support.transform));
  return hash;
}

// Aggregates a scene map into an order-stable content hash.
template <typename TMap, typename ValueHasher>
size_t HashSceneMapWithValues(const TMap &items, ValueHasher hasher) {
  size_t aggregate = std::hash<size_t>{}(items.size());
  for (const auto &[uuid, value] : items) {
    size_t entryHash = std::hash<std::string>{}(uuid);
    HashCombine(entryHash, hasher(value));
    aggregate ^= entryHash + 0x9e3779b9 + (aggregate << 6) + (aggregate >> 2);
  }
  return aggregate;
}
} // namespace

// Computes a scene-wide hash for data that can affect layout previews.
size_t LayoutViewerPanel::ComputeSceneContentHash() const {
  const auto &scene = GetDefaultGuiConfigServices().LegacyConfigManager().GetScene();
  size_t hash = 0;
  HashCombine(hash, HashSceneMapWithValues(scene.fixtures, HashFixtureValue));
  HashCombine(hash, HashSceneMapWithValues(scene.trusses, HashTrussValue));
  HashCombine(hash, HashSceneMapWithValues(scene.sceneObjects, HashSceneObjectValue));
  HashCombine(hash, HashSceneMapWithValues(scene.supports, HashSupportValue));
  return hash;
}

// Computes a view-specific hash for camera, render options, and layer filters.
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

// Marks cached layout rasters dirty only when content or LOD changes require it.
void LayoutViewerPanel::InvalidateRenderIfFrameChanged(bool includeSceneContent) {
  const double renderZoom = GetRenderZoom();
  const double pageWidth = currentLayout.pageSetup.PageWidthPt();
  const double pageHeight = currentLayout.pageSetup.PageHeightPt();
  const bool zoomChanged =
      ShouldRebuildCacheForRenderZoom(lastRenderZoom, renderZoom);
  const bool pageChanged =
      lastPageWidthPt != pageWidth || lastPageHeightPt != pageHeight;
  size_t sceneContentHash = lastSceneContentHash;
  bool sceneContentChanged = false;
  if (includeSceneContent) {
    sceneContentHash = ComputeSceneContentHash();
    sceneContentChanged =
        !hasSceneContentHash || sceneContentHash != lastSceneContentHash;
  }
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
    if (cache.texture == 0 ||
        ShouldRebuildCacheForRenderZoom(cache.renderZoom, renderZoom) ||
        renderSize != cache.textureSize) {
      markDirty(cache.renderDirty);
    }
  }

  for (const auto &legend : currentLayout.legendViews) {
    LegendCache &cache = GetLegendCache(legend.id);
    if (sceneContentChanged) {
      cache.symbols.reset();
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
    if (cache.texture == 0 ||
        ShouldRebuildCacheForRenderZoom(cache.renderZoom, renderZoom) ||
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
    if (cache.texture == 0 ||
        ShouldRebuildCacheForRenderZoom(cache.renderZoom, renderZoom) ||
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
    if (cache.texture == 0 ||
        ShouldRebuildCacheForRenderZoom(cache.renderZoom, renderZoom) ||
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
    if (cache.texture == 0 ||
        ShouldRebuildCacheForRenderZoom(cache.renderZoom, renderZoom) ||
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
  if (includeSceneContent) {
    lastSceneContentHash = sceneContentHash;
    hasSceneContentHash = true;
  }
}

// Invalidates render data after scene content changes and queues a rebuild.
void LayoutViewerPanel::RefreshAfterSceneContentUpdate() {
  legendDataDirty_ = true;
  RefreshLegendData();
  viewRenderVersion++;
  captureInProgress = false;

  for (auto &entry : viewCaches_) {
    ViewCache &cache = entry.second;
    cache.captureVersion = -1;
    cache.captureInProgress = false;
    cache.renderDirty = true;
  }

  for (auto &entry : legendCaches_) {
    entry.second.renderDirty = true;
    entry.second.symbols.reset();
  }

  renderDirty = true;
  InvalidateRenderIfFrameChanged(true);
  RequestRenderRebuild();
  Refresh();
}

// Refreshes the layout viewer after selection-only updates.
void LayoutViewerPanel::RefreshAfterSelectionOnlyUpdate() {
  Refresh();
}

// Invalidates render data after fixture symbol changes.
void LayoutViewerPanel::RefreshAfterFixtureSymbolUpdate() {
  RefreshAfterSceneContentUpdate();
}
