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

namespace {
void HashCombine(size_t &seed, size_t value) {
  seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

void HashCombineFloat(size_t &seed, float value) {
  HashCombine(seed, std::hash<float>{}(value));
}
} // namespace

size_t LayoutViewerPanel::HashViewContent(
    const layouts::Layout2DViewDefinition &view) const {
  size_t seed = std::hash<int>{}(view.id);
  HashCombine(seed, std::hash<float>{}(view.camera.offsetPixelsX));
  HashCombine(seed, std::hash<float>{}(view.camera.offsetPixelsY));
  HashCombine(seed, std::hash<float>{}(view.camera.zoom));
  HashCombine(seed, std::hash<int>{}(view.camera.view));
  HashCombine(seed, std::hash<int>{}(view.renderOptions.renderMode));
  HashCombine(seed, std::hash<bool>{}(view.renderOptions.darkMode));
  HashCombine(seed, std::hash<bool>{}(view.renderOptions.showGrid));
  HashCombine(seed, std::hash<int>{}(view.renderOptions.gridStyle));
  HashCombineFloat(seed, view.renderOptions.gridColorR);
  HashCombineFloat(seed, view.renderOptions.gridColorG);
  HashCombineFloat(seed, view.renderOptions.gridColorB);
  HashCombine(seed, std::hash<bool>{}(view.renderOptions.gridDrawAbove));

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
  return seed;
}

void LayoutViewerPanel::InvalidateRenderIfFrameChanged() {
  const double renderZoom = GetRenderZoom();
  const double pageWidth = currentLayout.pageSetup.PageWidthPt();
  const double pageHeight = currentLayout.pageSetup.PageHeightPt();
  const bool zoomChanged = lastRenderZoom != renderZoom;
  const bool pageChanged =
      lastPageWidthPt != pageWidth || lastPageHeightPt != pageHeight;
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

  if (zoomChanged || pageChanged) {
    renderDirty = true;
  }
  lastRenderZoom = renderZoom;
  lastPageWidthPt = pageWidth;
  lastPageHeightPt = pageHeight;
}
