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

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include <wx/gdicmn.h>

#include "canvas2d.h"
#include "symbolcache.h"
#include "viewer2dpanel.h"
#include "viewer2dstate.h"

class ConfigManager;
class Viewer2DOffscreenRenderer;

namespace layouts {
struct Layout2DViewDefinition;
}

namespace gui::layoutraster {

struct Layout2DViewRasterRequest {
  const layouts::Layout2DViewDefinition *view = nullptr;
  wxSize renderSize{0, 0};
  double renderZoom = 1.0;
  size_t contentHash = 0;
};

struct Layout2DViewRasterCacheInput {
  bool hasCapture = false;
  bool hasRenderState = false;
  bool restoredFromPersistentCache = false;
  const CommandBuffer *buffer = nullptr;
  const Viewer2DViewState *viewState = nullptr;
  const viewer2d::Viewer2DState *renderState = nullptr;
  const SymbolDefinitionSnapshot *symbols = nullptr;
  const std::vector<unsigned char> *persistentRgba = nullptr;
  wxSize persistentRgbaSize{0, 0};
  double persistentRgbaRenderZoom = 0.0;
  size_t persistentRgbaContentHash = 0;
};

struct Layout2DViewRasterResult {
  bool success = false;
  bool reusedPersistentRaster = false;
  bool renderedFromCommandBuffer = false;
  bool renderedFromCapturePanel = false;
  bool rejectedRestoredPersistentCache = false;
  int width = 0;
  int height = 0;
  std::vector<unsigned char> rgbaPixels;
  std::string diagnosticMessage;
};

class Layout2DViewRasterizer {
public:
  Layout2DViewRasterizer(ConfigManager &config,
                         Viewer2DOffscreenRenderer *offscreenRenderer,
                         Viewer2DPanel *capturePanel);

  Layout2DViewRasterResult
  Rasterize(const Layout2DViewRasterRequest &request,
            const Layout2DViewRasterCacheInput &cacheInput) const;

private:
  ConfigManager &config_;
  Viewer2DOffscreenRenderer *offscreenRenderer_ = nullptr;
  Viewer2DPanel *capturePanel_ = nullptr;
};

} // namespace gui::layoutraster
