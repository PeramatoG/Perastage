#pragma once

#include <optional>
#include <string_view>

namespace gui::gdtf {

// Stores an integer bitmap size without depending on a GUI toolkit.
struct SvgPreviewSize final {
  int width = 0;
  int height = 0;

  bool IsValid() const;
};

// Stores the authored SVG viewport dimensions used for aspect fitting.
struct SvgViewport final {
  double width = 0.0;
  double height = 0.0;
};

// Reads validated viewport dimensions from the SVG root element.
std::optional<SvgViewport> ParseSvgViewport(std::string_view svgText);

// Calculates an aspect-preserving SVG raster size bounded by the target.
SvgPreviewSize FitSvgPreviewSize(double sourceWidth, double sourceHeight,
                                 int targetWidth, int targetHeight,
                                 int maximumDimension);

} // namespace gui::gdtf
