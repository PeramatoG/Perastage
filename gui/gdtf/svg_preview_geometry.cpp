#include "gdtf/svg_preview_geometry.h"

#include <algorithm>
#include <cmath>

namespace gui::gdtf {

// Reports whether both fitted bitmap dimensions are usable.
bool SvgPreviewSize::IsValid() const { return width > 0 && height > 0; }

// Calculates an aspect-preserving SVG raster size bounded by the target.
SvgPreviewSize FitSvgPreviewSize(double sourceWidth, double sourceHeight,
                                 int targetWidth, int targetHeight,
                                 int maximumDimension) {
  if (!std::isfinite(sourceWidth) || !std::isfinite(sourceHeight) ||
      sourceWidth <= 0.0 || sourceHeight <= 0.0 || targetWidth <= 0 ||
      targetHeight <= 0 || maximumDimension <= 0 ||
      targetWidth > maximumDimension || targetHeight > maximumDimension)
    return {};

  const double scale =
      std::min(static_cast<double>(targetWidth) / sourceWidth,
               static_cast<double>(targetHeight) / sourceHeight);
  if (!std::isfinite(scale) || scale <= 0.0)
    return {};

  return {std::clamp(static_cast<int>(std::lround(sourceWidth * scale)), 1,
                     targetWidth),
          std::clamp(static_cast<int>(std::lround(sourceHeight * scale)), 1,
                     targetHeight)};
}

} // namespace gui::gdtf
