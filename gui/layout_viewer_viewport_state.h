#pragma once

#include <cstddef>
#include <vector>

namespace gui::layoutviewport {

inline constexpr double kMinZoom = 0.25;
inline constexpr double kMaxZoom = 10.0;
inline constexpr double kZoomStep = 1.1;
inline constexpr int kFitMarginPx = 40;
inline constexpr int kMinimumStableFitSizePx = 100;
inline constexpr int kMaxRenderDimension = 8192;
inline constexpr size_t kMaxRenderPixels =
    static_cast<size_t>(kMaxRenderDimension) * kMaxRenderDimension;
inline constexpr size_t kMaxRenderBytes = 64 * 1024 * 1024;

struct Point {
  int x = 0;
  int y = 0;
};

struct Size {
  int width = 0;
  int height = 0;
};

struct Rect {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
};

struct Frame {
  double x = 0.0;
  double y = 0.0;
  double width = 0.0;
  double height = 0.0;
};

class LayoutViewerViewportState {
public:
  double Zoom() const;
  Point PanOffset() const;
  void SetZoom(double zoom);
  void ApplyPan(Point delta);
  void Fit(Size viewport, double pageWidth, double pageHeight);
  bool ApplyWheelZoom(int rotation, int wheelDelta, Point pointer,
                      Size viewport, double safeMaxZoom);
  Rect PageRect(Size viewport, double pageWidth, double pageHeight) const;
  bool FrameRect(Size viewport, double pageWidth, double pageHeight,
                 Frame frame, Rect &rect) const;

  void RequestAutomaticFit();
  bool HasPendingAutomaticFit() const;
  bool ConsumeAutomaticFitIfReady(Size viewport, bool visible);
  void CancelAutomaticFit();

private:
  double zoom_ = 1.0;
  Point panOffset_;
  bool automaticFitPending_ = false;
};

double GetMaxZoomForFrame(Size frameSize);
double GetLayoutSafeMaxZoom(const std::vector<Size> &frameSizes);
bool IsViewportReadyForAutomaticFit(Size viewport, bool visible);

} // namespace gui::layoutviewport
