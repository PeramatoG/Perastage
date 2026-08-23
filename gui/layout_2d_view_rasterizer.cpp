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
#include "layout_2d_view_rasterizer.h"

#include <cmath>
#include <optional>

#include "LayoutCollection.h"
#include "layout_view_cache_archive.h"
#include "viewer2doffscreenrenderer.h"

namespace gui::layoutraster {
namespace {

// Returns the expected byte count for an RGBA raster of the given size.
size_t GetExpectedRgbaBytes(const wxSize &size) {
  if (size.GetWidth() <= 0 || size.GetHeight() <= 0)
    return 0;
  return static_cast<size_t>(size.GetWidth()) *
         static_cast<size_t>(size.GetHeight()) * 4;
}

// Checks whether the persistent RGBA cache exactly matches this raster request.
bool HasMatchingPersistentRaster(const Layout2DViewRasterRequest &request,
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

// Restores temporary capture-panel overrides when rasterization leaves scope.
struct ScopedCapturePanelRestore {
  Viewer2DPanel *panel = nullptr;
  std::optional<Viewer2DRenderOverrides> overrides;
  bool preferLayoutSvgSymbols = false;

  // Restores the capture panel state saved before preview rasterization.
  ~ScopedCapturePanelRestore() {
    if (!panel)
      return;
    panel->SetRenderOverrides(overrides);
    panel->SetPreferPerastageSvgSymbolsForLayouts(preferLayoutSvgSymbols);
  }
};

} // namespace

// Creates a rasterizer around the existing offscreen Viewer2D render path.
Layout2DViewRasterizer::Layout2DViewRasterizer(
    ConfigManager &config, Viewer2DOffscreenRenderer *offscreenRenderer,
    Viewer2DPanel *capturePanel)
    : config_(config), offscreenRenderer_(offscreenRenderer),
      capturePanel_(capturePanel) {}

// Rasterizes one Layout 2D view into RGBA pixels without owning GL textures.
Layout2DViewRasterResult Layout2DViewRasterizer::Rasterize(
    const Layout2DViewRasterRequest &request,
    const Layout2DViewRasterCacheInput &cacheInput) const {
  Layout2DViewRasterResult result;
  if (!request.view) {
    result.failureReason =
        Layout2DViewRasterFailureReason::MissingViewDefinition;
    result.diagnosticMessage = "missing 2D view definition";
    return result;
  }
  if (request.renderSize.GetWidth() <= 0 || request.renderSize.GetHeight() <= 0) {
    result.failureReason = Layout2DViewRasterFailureReason::InvalidFrameSize;
    result.diagnosticMessage = "invalid frame size";
    return result;
  }
  std::string persistentDiagnostic;
  if (HasMatchingPersistentRaster(request, cacheInput, persistentDiagnostic)) {
    result.rgbaPixels = *cacheInput.persistentRgba;
    result.width = request.renderSize.GetWidth();
    result.height = request.renderSize.GetHeight();
    result.success = true;
    result.reusedPersistentRaster = true;
    return result;
  }

  if (!cacheInput.hasCapture || !cacheInput.hasRenderState) {
    result.failureReason = !cacheInput.hasCapture
                               ? Layout2DViewRasterFailureReason::MissingCaptureData
                               : Layout2DViewRasterFailureReason::MissingRenderState;
    result.diagnosticMessage = "missing capture data or render state";
    return result;
  }

  if (cacheInput.restoredFromPersistentCache) {
    if (!cacheInput.buffer || !cacheInput.viewState) {
      result.rejectedRestoredPersistentCache = true;
      result.failureReason =
          Layout2DViewRasterFailureReason::MissingCaptureData;
      result.diagnosticMessage = "missing command-buffer cache data";
    } else if (cacheInput.buffer->commands.empty()) {
      result.rejectedRestoredPersistentCache = true;
      result.failureReason =
          Layout2DViewRasterFailureReason::EmptyCommandBuffer;
      result.diagnosticMessage = "empty command buffer";
    } else if (gui::layoutcache::RenderCommandBufferCacheToRgba(
                   request.renderSize, *cacheInput.buffer, *cacheInput.viewState,
                   cacheInput.symbols, request.renderZoom, result.rgbaPixels,
                   result.width, result.height)) {
      result.success = true;
      result.renderedFromCommandBuffer = true;
      return result;
    } else {
      result.rejectedRestoredPersistentCache = true;
      result.failureReason =
          Layout2DViewRasterFailureReason::CommandBufferRasterizationFailed;
      result.diagnosticMessage = "RenderCommandBufferCacheToRgba failed";
    }
  }

  if (!capturePanel_) {
    result.failureReason = Layout2DViewRasterFailureReason::MissingCapturePanel;
    result.diagnosticMessage = "missing capture panel";
    return result;
  }
  if (!offscreenRenderer_) {
    result.failureReason =
        Layout2DViewRasterFailureReason::MissingOffscreenRenderer;
    result.diagnosticMessage = "missing offscreen renderer";
    return result;
  }
  if (!cacheInput.renderState) {
    result.failureReason = Layout2DViewRasterFailureReason::MissingRenderState;
    result.diagnosticMessage = "missing render state";
    return result;
  }

  offscreenRenderer_->SetViewportSize(request.renderSize);
  offscreenRenderer_->PrepareForCapture();

  viewer2d::Viewer2DState renderState = *cacheInput.renderState;
  if (request.renderZoom != 1.0) {
    renderState.camera.zoom *= static_cast<float>(request.renderZoom);
  }
  renderState.camera.viewportWidth = request.renderSize.GetWidth();
  renderState.camera.viewportHeight = request.renderSize.GetHeight();

  viewer2d::ScopedViewer2DState stateGuard(capturePanel_, nullptr, config_,
                                           renderState, nullptr, nullptr,
                                           false);
  const auto previousOverrides = capturePanel_->GetRenderOverrides();
  const bool previousPreferLayoutSvgSymbols =
      capturePanel_->GetPreferPerastageSvgSymbolsForLayouts();
  ScopedCapturePanelRestore scopedRestore{capturePanel_, previousOverrides,
                                           previousPreferLayoutSvgSymbols};

  Viewer2DRenderOverrides previewOverrides =
      previousOverrides.value_or(Viewer2DRenderOverrides{});
  previewOverrides.drawFixtureLabels = true;
  previewOverrides.symbolCaptureRenderProfile = false;
  capturePanel_->SetRenderOverrides(previewOverrides);
  capturePanel_->SetPreferPerastageSvgSymbolsForLayouts(true);

  if (!capturePanel_->RenderToRGBA(result.rgbaPixels, result.width,
                                   result.height, request.renderSize)) {
    result.failureReason = Layout2DViewRasterFailureReason::RenderToRgbaFailed;
    result.diagnosticMessage = "Viewer2DPanel::RenderToRGBA failed";
    return result;
  }
  if (result.width <= 0 || result.height <= 0 || result.rgbaPixels.empty()) {
    result.failureReason =
        Layout2DViewRasterFailureReason::InvalidRgbaDimensions;
    result.diagnosticMessage = "invalid RGBA dimensions";
    return result;
  }
  result.success = true;
  result.renderedFromCapturePanel = true;
  return result;
}

} // namespace gui::layoutraster
