#include "viewer2dviewfit.h"

#include "iselectioncontext.h"
#include <algorithm>
#include <limits>

namespace {
constexpr float kPixelsPerMeter = 25.0f;
constexpr float kMinZoom = 0.1f;
constexpr float kFitPaddingScale = 1.15f;

void ExpandProjectedBounds(const ISelectionContext::BoundingBox &box,
                           Viewer2DView view, float &minA, float &maxA,
                           float &minB, float &maxB) {
  float axisAMin = 0.0f;
  float axisAMax = 0.0f;
  float axisBMin = 0.0f;
  float axisBMax = 0.0f;

  switch (view) {
  case Viewer2DView::Top:
  case Viewer2DView::Bottom:
    axisAMin = box.min[0];
    axisAMax = box.max[0];
    axisBMin = box.min[1];
    axisBMax = box.max[1];
    break;
  case Viewer2DView::Front:
    axisAMin = box.min[0];
    axisAMax = box.max[0];
    axisBMin = box.min[2];
    axisBMax = box.max[2];
    break;
  case Viewer2DView::Side:
    // Side view projects world Y onto screen X with a mirrored sign.
    // Convert [minY, maxY] into the projected range [-maxY, -minY]
    // so fitting and centering match the rendered orientation.
    axisAMin = -box.max[1];
    axisAMax = -box.min[1];
    axisBMin = box.min[2];
    axisBMax = box.max[2];
    break;
  }

  minA = std::min(minA, axisAMin);
  maxA = std::max(maxA, axisAMax);
  minB = std::min(minB, axisBMin);
  maxB = std::max(maxB, axisBMax);
}

} // namespace

namespace viewer2d {

bool ComputeViewFit(const ISelectionContext &selectionContext, Viewer2DView view,
                    int viewportWidth, int viewportHeight,
                    ViewFitResult &result) {
  if (viewportWidth <= 0 || viewportHeight <= 0)
    return false;

  float minA = std::numeric_limits<float>::max();
  float maxA = std::numeric_limits<float>::lowest();
  float minB = std::numeric_limits<float>::max();
  float maxB = std::numeric_limits<float>::lowest();

  auto accumulate = [&](const auto &map) {
    for (const auto &[uuid, bounds] : map) {
      (void)uuid;
      ExpandProjectedBounds(bounds, view, minA, maxA, minB, maxB);
    }
  };

  accumulate(selectionContext.GetFixtureBoundsMap());
  accumulate(selectionContext.GetTrussBoundsMap());
  accumulate(selectionContext.GetObjectBoundsMap());

  if (minA > maxA || minB > maxB)
    return false;

  const float centerA = (minA + maxA) * 0.5f;
  const float centerB = (minB + maxB) * 0.5f;
  const float spanA = std::max(maxA - minA, 0.25f);
  const float spanB = std::max(maxB - minB, 0.25f);

  const float requiredHalfA = spanA * 0.5f * kFitPaddingScale;
  const float requiredHalfB = spanB * 0.5f * kFitPaddingScale;

  const float zoomForWidth =
      static_cast<float>(viewportWidth) / (2.0f * kPixelsPerMeter * requiredHalfA);
  const float zoomForHeight = static_cast<float>(viewportHeight) /
                              (2.0f * kPixelsPerMeter * requiredHalfB);

  result.zoom = std::max(kMinZoom, std::min(zoomForWidth, zoomForHeight));
  result.offsetXPixels = -centerA * kPixelsPerMeter;
  result.offsetYPixels = -centerB * kPixelsPerMeter;
  return true;
}

} // namespace viewer2d
