#include "gdtf/gdtf_resource_bitmap_cache.h"
#include "gdtf/svg_preview_geometry.h"

#include <cassert>
#include <cmath>

#include <wx/init.h>

namespace {

// Reports whether one bitmap pixel is close to the expected authored colour.
bool PixelNear(const wxImage &image, int x, int y, int red, int green,
               int blue, int tolerance = 4) {
  return std::abs(static_cast<int>(image.GetRed(x, y)) - red) <= tolerance &&
         std::abs(static_cast<int>(image.GetGreen(x, y)) - green) <= tolerance &&
         std::abs(static_cast<int>(image.GetBlue(x, y)) - blue) <= tolerance;
}

// Reports whether one pixel contains either stable checkerboard colour.
bool IsCheckerPixel(const wxImage &image, int x, int y) {
  return PixelNear(image, x, y, 210, 210, 210) ||
         PixelNear(image, x, y, 145, 145, 145);
}

// Verifies deterministic aspect-fit geometry independently of rasterization.
void TestFittedSizes() {
  using gui::gdtf::FitSvgPreviewSize;
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
}

// Renders one SVG through the production cache and requires a valid result.
wxImage RenderSvg(GdtfResourceBitmapCache &cache, const std::string &entry,
                  const std::string &svg, const wxSize &target) {
  const auto result =
      cache.GetOrCreateSvg("source", entry, svg, target, *wxWHITE);
  assert(result.decoded && result.bitmap.IsOk());
  assert(result.bitmap.GetSize() == target);
  return result.bitmap.ConvertToImage();
}

} // namespace

// Verifies SVG fitting, alpha composition, authored white, and safe failures.
int main() {
  wxInitializer initializer;
  if (!initializer.IsOk())
    return 77;
  GdtfResourceBitmapCache cache;
  TestFittedSizes();

  const std::string portrait =
      "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 200'>"
      "<rect width='100' height='200' fill='#ff0000'/></svg>";
  const wxImage portraitImage =
      RenderSvg(cache, "portrait.svg", portrait, wxSize(200, 100));
  assert(IsCheckerPixel(portraitImage, 20, 50));
  assert(PixelNear(portraitImage, 100, 50, 255, 0, 0));
  assert(IsCheckerPixel(portraitImage, 180, 50));

  const std::string landscape =
      "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 200 100'>"
      "<rect width='200' height='100' fill='#00ff00'/></svg>";
  const wxImage landscapeImage =
      RenderSvg(cache, "landscape.svg", landscape, wxSize(100, 200));
  assert(IsCheckerPixel(landscapeImage, 50, 20));
  assert(PixelNear(landscapeImage, 50, 100, 0, 255, 0));
  assert(IsCheckerPixel(landscapeImage, 50, 180));

  const std::string square =
      "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'>"
      "<rect width='100' height='100' fill='#0000ff'/></svg>";
  const wxImage squareImage =
      RenderSvg(cache, "square.svg", square, wxSize(200, 100));
  assert(IsCheckerPixel(squareImage, 20, 50));
  assert(PixelNear(squareImage, 100, 50, 0, 0, 255));
  assert(IsCheckerPixel(squareImage, 180, 50));

  const std::string transparent =
      "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'>"
      "<rect width='50' height='100' fill='#ff0000'/></svg>";
  const wxImage transparentImage =
      RenderSvg(cache, "transparent.svg", transparent, wxSize(100, 100));
  assert(PixelNear(transparentImage, 25, 50, 255, 0, 0));
  assert(IsCheckerPixel(transparentImage, 75, 50));

  const std::string white =
      "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'>"
      "<rect width='100' height='100' fill='#ffffff'/></svg>";
  const wxImage whiteImage =
      RenderSvg(cache, "white.svg", white, wxSize(100, 100));
  assert(PixelNear(whiteImage, 50, 50, 255, 255, 255));

  const auto invalid = cache.GetOrCreateSvg("source", "broken.svg", "not svg",
                                            wxSize(64, 48), *wxWHITE);
  assert(!invalid.decoded && invalid.bitmap.IsOk());
  const auto oversized = cache.GetOrCreateSvg("source", "large.svg", square,
                                              wxSize(5000, 48), *wxWHITE);
  assert(!oversized.decoded);
  assert(oversized.status == GdtfBitmapDecodeStatus::DimensionsTooLarge);
  return 0;
}
