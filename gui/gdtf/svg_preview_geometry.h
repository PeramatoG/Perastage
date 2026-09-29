#pragma once

namespace gui::gdtf {

// Stores an integer bitmap size without depending on a GUI toolkit.
struct SvgPreviewSize final {
  int width = 0;
  int height = 0;

  bool IsValid() const;
};

// Calculates an aspect-preserving SVG raster size bounded by the target.
SvgPreviewSize FitSvgPreviewSize(double sourceWidth, double sourceHeight,
                                 int targetWidth, int targetHeight,
                                 int maximumDimension);

} // namespace gui::gdtf
