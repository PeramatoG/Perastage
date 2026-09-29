#include "gdtf/svg_preview_geometry.h"

#include <cassert>
#include <cmath>
#include <limits>

using gui::gdtf::FitSvgPreviewSize;
using gui::gdtf::ParseSvgViewport;

// Verifies authored SVG viewport parsing without a GUI runtime.
void TestViewportParsing() {
  auto viewport = ParseSvgViewport(
      "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 456 502.71'/>");
  assert(viewport && viewport->width == 456.0);
  assert(std::abs(viewport->height - 502.71) < 0.0001);
  viewport = ParseSvgViewport("<svg viewBox='10, -20, 100, 200'/>");
  assert(viewport && viewport->width == 100.0 && viewport->height == 200.0);
  viewport = ParseSvgViewport("<svg viewBox='0 0 200 100'/>");
  assert(viewport && viewport->width == 200.0 && viewport->height == 100.0);
  viewport = ParseSvgViewport("<svg viewBox='0 0 100 100'/>");
  assert(viewport && viewport->width == 100.0 && viewport->height == 100.0);
  viewport = ParseSvgViewport("<svg width='2in' height='25.4mm'/>");
  assert(viewport && viewport->width == 192.0);
  assert(std::abs(viewport->height - 96.0) < 0.0001);
  assert(!ParseSvgViewport("<svg viewBox='0 0 100'/>").has_value());
  assert(!ParseSvgViewport("<svg viewBox='0 0 0 100'/>").has_value());
  assert(!ParseSvgViewport("<svg viewBox='0 0 -1 100'/>").has_value());
  assert(!ParseSvgViewport("<svg viewBox='0 0 nan 100'/>").has_value());
  assert(!ParseSvgViewport("<svg viewBox='0 0 inf 100'/>").has_value());
  assert(!ParseSvgViewport("<not-svg viewBox='0 0 100 100'/>").has_value());
}

// Verifies deterministic aspect fitting and invalid-dimension rejection.
void TestFittedSizes() {
  auto fitted = FitSvgPreviewSize(100.0, 200.0, 200, 100, 4096);
  assert(fitted.width == 50 && fitted.height == 100);
  fitted = FitSvgPreviewSize(200.0, 100.0, 100, 200, 4096);
  assert(fitted.width == 100 && fitted.height == 50);
  fitted = FitSvgPreviewSize(100.0, 100.0, 200, 100, 4096);
  assert(fitted.width == 100 && fitted.height == 100);
  fitted = FitSvgPreviewSize(1.0, 10000.0, 100, 100, 4096);
  assert(fitted.width == 1 && fitted.height == 100);
  assert(!FitSvgPreviewSize(0.0, 100.0, 100, 100, 4096).IsValid());
  assert(!FitSvgPreviewSize(100.0, 100.0, 0, 100, 4096).IsValid());
  assert(!FitSvgPreviewSize(100.0, 100.0, 5000, 100, 4096).IsValid());
  assert(!FitSvgPreviewSize(std::numeric_limits<double>::infinity(), 100.0,
                            100, 100, 4096)
              .IsValid());
}

// Runs pure SVG viewport and geometry coverage without display initialization.
int main() {
  TestViewportParsing();
  TestFittedSizes();
  return 0;
}
