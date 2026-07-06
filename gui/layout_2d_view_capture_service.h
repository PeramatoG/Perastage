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
#include <functional>
#include <memory>

#include "LayoutCollection.h"
#include "canvas2d.h"
#include "symbolcache.h"
#include "viewer2dpanel.h"
#include "viewer2dstate.h"

class Viewer2DOffscreenRenderer;

namespace gui::layoutcapture {

struct Layout2DViewCaptureRequest {
  const layouts::Layout2DViewDefinition *view = nullptr;
  Viewer2DPanel *capturePanel = nullptr;
  Viewer2DOffscreenRenderer *offscreenRenderer = nullptr;
  int viewRenderVersion = 0;
  size_t contentHash = 0;
  bool panelCaptureInProgress = false;
  bool cacheCaptureInProgress = false;
  bool cacheHasCapture = false;
  bool cacheHasRenderState = false;
  bool cacheHasCaptureContentHash = false;
  size_t cacheCaptureContentHash = 0;
  int cacheCaptureVersion = -1;
  bool panelIsDestroying = false;
};

struct Layout2DViewCaptureCallbacks {
  std::function<void(bool)> setPanelCaptureInProgress;
  std::function<void(bool)> setCacheCaptureInProgress;
  std::function<void(const viewer2d::Viewer2DState &)> storeRenderState;
  std::function<void(CommandBuffer, Viewer2DViewState,
                     std::shared_ptr<const SymbolDefinitionSnapshot>, int, int,
                     size_t, int)>
      completeCapture;
};

struct Layout2DViewCaptureScheduleResult {
  bool captureNeeded = false;
  bool captureScheduled = false;
};

Layout2DViewCaptureScheduleResult ScheduleLayout2DViewCaptureIfNeeded(
    const Layout2DViewCaptureRequest &request,
    const Layout2DViewCaptureCallbacks &callbacks);

} // namespace gui::layoutcapture
