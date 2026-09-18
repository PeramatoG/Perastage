#include "layout_viewer_viewport_state.h"

#include <algorithm>
#include <cmath>

namespace gui::layoutviewport {

// Returns the current logical viewport zoom.
double LayoutViewerViewportState::Zoom() const { return zoom_; }

// Returns the accumulated logical viewport pan offset.
Point LayoutViewerViewportState::PanOffset() const { return panOffset_; }

// Sets the zoom while enforcing the global viewport limits.
void LayoutViewerViewportState::SetZoom(double zoom) {
  zoom_ = std::clamp(zoom, kMinZoom, kMaxZoom);
}

// Applies a gesture-produced delta to the logical pan offset.
void LayoutViewerViewportState::ApplyPan(Point delta) {
  panOffset_.x += delta.x;
  panOffset_.y += delta.y;
}

// Fits and centers a page in the supplied viewport.
void LayoutViewerViewportState::Fit(Size viewport, double pageWidth,
                                    double pageHeight) {
  if (pageWidth <= 0.0 || pageHeight <= 0.0 || viewport.width <= 0 ||
      viewport.height <= 0) {
    zoom_ = 1.0;
    panOffset_ = {};
    return;
  }
  const double fitWidth =
      static_cast<double>(viewport.width - kFitMarginPx) / pageWidth;
  const double fitHeight =
      static_cast<double>(viewport.height - kFitMarginPx) / pageHeight;
  zoom_ = std::clamp(std::min(fitWidth, fitHeight), kMinZoom, kMaxZoom);
  panOffset_ = {};
}

// Applies pointer-anchored wheel zoom and reports whether state changed.
bool LayoutViewerViewportState::ApplyWheelZoom(int rotation, int wheelDelta,
                                               Point pointer, Size viewport,
                                               double safeMaxZoom) {
  if (wheelDelta == 0 || rotation == 0)
    return false;
  const double steps =
      static_cast<double>(rotation) / static_cast<double>(wheelDelta);
  const double factor = std::pow(kZoomStep, steps);
  const double newZoom =
      std::clamp(zoom_ * factor, kMinZoom, safeMaxZoom);
  if (std::abs(newZoom - zoom_) < 1e-6)
    return false;
  const Point center{viewport.width / 2, viewport.height / 2};
  const Point relative{pointer.x - center.x - panOffset_.x,
                       pointer.y - center.y - panOffset_.y};
  const double scale = newZoom / zoom_;
  const Point newRelative{static_cast<int>(relative.x * scale),
                          static_cast<int>(relative.y * scale)};
  panOffset_.x += relative.x - newRelative.x;
  panOffset_.y += relative.y - newRelative.y;
  zoom_ = newZoom;
  return true;
}

// Calculates the page rectangle using the current zoom and pan state.
Rect LayoutViewerViewportState::PageRect(Size viewport, double pageWidth,
                                         double pageHeight) const {
  const double scaledWidth = pageWidth * zoom_;
  const double scaledHeight = pageHeight * zoom_;
  const Point center{viewport.width / 2, viewport.height / 2};
  return {center.x - static_cast<int>(scaledWidth / 2.0) + panOffset_.x,
          center.y - static_cast<int>(scaledHeight / 2.0) + panOffset_.y,
          static_cast<int>(scaledWidth), static_cast<int>(scaledHeight)};
}

// Maps page-relative frame geometry into viewport coordinates.
bool LayoutViewerViewportState::FrameRect(Size viewport, double pageWidth,
                                          double pageHeight, Frame frame,
                                          Rect &rect) const {
  if (frame.width <= 0.0 || frame.height <= 0.0)
    return false;
  const Rect page = PageRect(viewport, pageWidth, pageHeight);
  rect = {page.x + static_cast<int>(std::lround(frame.x * zoom_)),
          page.y + static_cast<int>(std::lround(frame.y * zoom_)),
          static_cast<int>(std::lround(frame.width * zoom_)),
          static_cast<int>(std::lround(frame.height * zoom_))};
  return true;
}

// Marks an automatic fit request as pending.
void LayoutViewerViewportState::RequestAutomaticFit() {
  automaticFitPending_ = true;
}

// Reports whether an automatic fit request remains pending.
bool LayoutViewerViewportState::HasPendingAutomaticFit() const {
  return automaticFitPending_;
}

// Consumes a pending fit exactly once when the viewport is ready.
bool LayoutViewerViewportState::ConsumeAutomaticFitIfReady(Size viewport,
                                                           bool visible) {
  if (!automaticFitPending_ ||
      !IsViewportReadyForAutomaticFit(viewport, visible))
    return false;
  automaticFitPending_ = false;
  return true;
}

// Cancels any pending automatic fit request.
void LayoutViewerViewportState::CancelAutomaticFit() {
  automaticFitPending_ = false;
}

// Computes a safe zoom cap from render dimensions and the RGBA byte budget.
double GetMaxZoomForFrame(Size frameSize) {
  if (frameSize.width <= 0 || frameSize.height <= 0)
    return kMaxZoom;
  const double width = static_cast<double>(frameSize.width);
  const double height = static_cast<double>(frameSize.height);
  const double byDimension =
      std::min(static_cast<double>(kMaxRenderDimension) / width,
               static_cast<double>(kMaxRenderDimension) / height);
  const double maxPixelsByArea =
      static_cast<double>(kMaxRenderBytes / 4) / (width * height);
  if (maxPixelsByArea <= 0.0)
    return std::clamp(byDimension, kMinZoom, kMaxZoom);
  return std::clamp(std::min(byDimension, std::sqrt(maxPixelsByArea)),
                    kMinZoom, kMaxZoom);
}

// Computes the layout-wide safe zoom cap across renderable frame sizes.
double GetLayoutSafeMaxZoom(const std::vector<Size> &frameSizes) {
  double maxZoom = kMaxZoom;
  for (const Size frameSize : frameSizes)
    maxZoom = std::min(maxZoom, GetMaxZoomForFrame(frameSize));
  return std::clamp(maxZoom, kMinZoom, kMaxZoom);
}

// Reports whether visible viewport geometry is stable enough for fitting.
bool IsViewportReadyForAutomaticFit(Size viewport, bool visible) {
  return visible && viewport.width >= kMinimumStableFitSizePx &&
         viewport.height >= kMinimumStableFitSizePx;
}

} // namespace gui::layoutviewport
