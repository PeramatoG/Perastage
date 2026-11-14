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
#pragma once
#include "layout_2d_view_rasterizer.h"
#include "LayoutCollection.h"
#include <cmath>
#include <string>
namespace gui::layoutraster {
// Returns the expected byte count for an RGBA raster of the given size.
inline size_t GetExpectedRgbaBytes(const wxSize &size) {
  if (size.GetWidth() <= 0 || size.GetHeight() <= 0)
    return 0;
  return static_cast<size_t>(size.GetWidth()) *
         static_cast<size_t>(size.GetHeight()) * 4;
}

// Checks whether the persistent RGBA cache exactly matches this raster request.
inline bool HasMatchingPersistentRaster(const Layout2DViewRasterRequest &request,
                                 const Layout2DViewRasterCacheInput &cacheInput,
                                 std::string &diagnostic) {
  if (!cacheInput.persistentRgba) {
    diagnostic = "missing persistent raster data";
    return false;
  }
  const size_t expectedBytes = GetExpectedRgbaBytes(request.renderSize);
  if (cacheInput.persistentRgbaSize != request.renderSize) {
    diagnostic = "persistent raster rejected due to size mismatch";
    return false;
  }
  if (cacheInput.persistentRgbaContentHash != request.contentHash) {
    diagnostic = "persistent raster rejected due to content hash mismatch";
    return false;
  }
  if (std::abs(cacheInput.persistentRgbaRenderZoom - request.renderZoom) >=
      0.000001) {
    diagnostic = "persistent raster rejected due to zoom mismatch";
    return false;
  }
  if (cacheInput.persistentRgba->size() != expectedBytes) {
    diagnostic = "persistent raster rejected due to invalid RGBA byte count";
    return false;
  }
  diagnostic.clear();
  return true;
}

inline bool UseCommandBufferRaster(const Layout2DViewRasterRequest &request,
                                  const Layout2DViewRasterCacheInput &cache) {
  return request.view && request.view->scaleMode == layouts::ViewportScaleMode::Free &&
         cache.restoredFromPersistentCache;
}
inline bool NeedsLiveViewportScene(const Layout2DViewRasterRequest &request,
                                 const Layout2DViewRasterCacheInput &cache) {
  std::string diagnostic;
  return !HasMatchingPersistentRaster(request,cache,diagnostic) &&
         !UseCommandBufferRaster(request,cache);
}
}
