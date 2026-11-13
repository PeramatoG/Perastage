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
#include "layoutviewerpanel_helpers.h"
#include <cmath>
using gui::layoutviewport::kMaxRenderBytes;
using gui::layoutviewport::kMaxRenderDimension;
using gui::layoutviewport::kMaxRenderPixels;

// Adapts pure frame geometry to the wx rectangle used for hit testing.
bool LayoutViewerPanel::GetFrameRect(const layouts::ViewportFramePoints &frame,
                                     wxRect &rect) const {
  const wxSize size = layoutviewerpanel::GetLogicalClientSize(this);
  gui::layoutviewport::Rect viewportRect;
  if (!viewportState_.FrameRect(
          {size.GetWidth(), size.GetHeight()},
          currentLayout.pageSetup.PageWidthPt(),
          currentLayout.pageSetup.PageHeightPt(),
          {static_cast<double>(frame.x), static_cast<double>(frame.y),
           static_cast<double>(frame.width), static_cast<double>(frame.height)},
          viewportRect))
    return false;
  rect = wxRect(viewportRect.x, viewportRect.y, viewportRect.width,
                viewportRect.height);
  return true;
}

// Calculates a raster frame size for the requested cache zoom.
wxSize
LayoutViewerPanel::GetFrameSizeForZoom(const layouts::ViewportFramePoints &frame,
                                       double targetZoom) const {
  if (!std::isfinite(frame.width) || !std::isfinite(frame.height) || !std::isfinite(targetZoom) ||
      frame.width <= 0 || frame.height <= 0 || targetZoom <= 0.0)
    return wxSize(0, 0);
  const double scaledWidthValue = frame.width * targetZoom;
  const double scaledHeightValue = frame.height * targetZoom;
  if (scaledWidthValue > kMaxRenderDimension ||
      scaledHeightValue > kMaxRenderDimension)
    return wxSize(0, 0);
  const int scaledWidth = static_cast<int>(std::lround(scaledWidthValue));
  const int scaledHeight = static_cast<int>(std::lround(scaledHeightValue));
  if (scaledWidth <= 0 || scaledHeight <= 0)
    return wxSize(0, 0);
  if (scaledWidth > kMaxRenderDimension || scaledHeight > kMaxRenderDimension)
    return wxSize(0, 0);
  if (static_cast<size_t>(scaledWidth) >
      kMaxRenderPixels / static_cast<size_t>(scaledHeight))
    return wxSize(0, 0);
  const size_t pixelCount =
      static_cast<size_t>(scaledWidth) * static_cast<size_t>(scaledHeight);
  if (pixelCount > kMaxRenderPixels || pixelCount > kMaxRenderBytes / 4)
    return wxSize(0, 0);
  return wxSize(scaledWidth, scaledHeight);
}

// Applies the view's fractional point residuals to integer gesture previews.
layouts::ViewportFramePoints LayoutViewerPanel::GetDisplayedFrame(
    gui::layoutselection::LayoutElementRef element,
    const layouts::Layout2DViewFrame &fallback) const {
  if (element.kind == LayoutElementKind::View2D) {
    for (const auto &view : currentLayout.view2dViews) if (view.id == element.id) {
      auto precise = layouts::ResolveViewportFrame(view);
      precise.x += double(fallback.x) - view.frame.x;
      precise.y += double(fallback.y) - view.frame.y;
      precise.width += double(fallback.width) - view.frame.width;
      precise.height += double(fallback.height) - view.frame.height;
      return precise;
    }
  }
  return static_cast<layouts::ViewportFramePoints>(fallback);
}
