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
#include "layout_2d_view_capture_service.h"

#include <memory>
#include <utility>

#include <wx/log.h>

#include "configmanager.h"
#include "guiconfigservices.h"
#include "viewer2doffscreenrenderer.h"
#include "viewer2dpanel.h"

namespace gui::layoutcapture {
namespace {

// Returns whether the cached command buffer must be refreshed for this view.
bool NeedsCapture(const Layout2DViewCaptureRequest &request) {
  return !request.cacheHasCapture || !request.cacheHasRenderState ||
         request.cacheCaptureVersion != request.viewRenderVersion ||
         !request.cacheHasCaptureContentHash ||
         request.cacheCaptureContentHash != request.contentHash;
}

} // namespace

// Schedules a capture for one embedded Layout 2D view when required.
Layout2DViewCaptureScheduleResult ScheduleLayout2DViewCaptureIfNeeded(
    const Layout2DViewCaptureRequest &request,
    const Layout2DViewCaptureCallbacks &callbacks) {
  Layout2DViewCaptureScheduleResult result;
  result.captureNeeded = NeedsCapture(request);
  if (!result.captureNeeded)
    return result;

  if (!request.view || request.panelIsDestroying) {
    wxLogTrace("layout2d.capture",
               "Skipping Layout 2D capture because the view is unavailable.");
    return result;
  }
  if (!request.capturePanel) {
    wxLogTrace("layout2d.capture",
               "Skipping Layout 2D capture for view %d because capture panel "
               "is unavailable.",
               request.view->id);
    return result;
  }
  if (request.panelCaptureInProgress || request.cacheCaptureInProgress)
    return result;
  if (!callbacks.setPanelCaptureInProgress ||
      !callbacks.setCacheCaptureInProgress || !callbacks.storeRenderState ||
      !callbacks.completeCapture) {
    wxLogTrace("layout2d.capture",
               "Skipping Layout 2D capture for view %d because callbacks are "
               "incomplete.",
               request.view->id);
    return result;
  }

  const int fallbackViewportWidth = request.view->camera.viewportWidth > 0
                                        ? request.view->camera.viewportWidth
                                        : request.view->frame.width;
  const int fallbackViewportHeight = request.view->camera.viewportHeight > 0
                                         ? request.view->camera.viewportHeight
                                         : request.view->frame.height;
  if (fallbackViewportWidth <= 0 || fallbackViewportHeight <= 0) {
    wxLogTrace("layout2d.capture",
               "Skipping Layout 2D capture for view %d because the viewport is "
               "invalid.",
               request.view->id);
    return result;
  }

  callbacks.setPanelCaptureInProgress(true);
  callbacks.setCacheCaptureInProgress(true);

  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  viewer2d::Viewer2DState layoutState =
      viewer2d::FromLayoutDefinition(*request.view);
  layoutState.renderOptions.darkMode = false;
  callbacks.storeRenderState(layoutState);

  if (request.offscreenRenderer) {
    request.offscreenRenderer->SetViewportSize(
        wxSize(fallbackViewportWidth, fallbackViewportHeight));
    request.offscreenRenderer->PrepareForCapture();
  } else {
    wxLogTrace("layout2d.capture",
               "Scheduling Layout 2D capture for view %d without offscreen "
               "renderer preparation.",
               request.view->id);
  }

  auto stateGuard = std::make_shared<viewer2d::ScopedViewer2DState>(
      request.capturePanel, nullptr, cfg, layoutState, nullptr, nullptr, false);
  Viewer2DPanel *capturePanel = request.capturePanel;
  const size_t captureContentHash = request.contentHash;
  const int captureVersion = request.viewRenderVersion;
  capturePanel->CaptureFrameNow(
      [callbacks, stateGuard, capturePanel, fallbackViewportWidth,
       fallbackViewportHeight, captureContentHash,
       captureVersion](CommandBuffer buffer, Viewer2DViewState state) {
        std::shared_ptr<const SymbolDefinitionSnapshot> symbols;
        if (capturePanel)
          symbols = capturePanel->GetBottomSymbolCacheSnapshot();
        callbacks.completeCapture(std::move(buffer), state, std::move(symbols),
                                  fallbackViewportWidth, fallbackViewportHeight,
                                  captureContentHash, captureVersion);
      },
      true, true);
  result.captureScheduled = true;
  return result;
}

} // namespace gui::layoutcapture
