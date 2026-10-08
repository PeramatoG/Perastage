#include "gdtf/gdtf_resource_bitmap_cache.h"
#include "symbols/Symbol2DSvg.h"
#include "windows/symbol_preview_drawing.h"
#include <wx/dcmemory.h>
#include <memory>
#include <cassert>
#include <cmath>

#include <wx/app.h>
#include <wx/bmpbndl.h>

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

class TestApp final : public wxApp {
public:
  // Initializes the GUI runtime required by bitmap and memory-DC resources.
  bool OnInit() override { return true; }
};

wxIMPLEMENT_APP_NO_MAIN(TestApp);

class AppScope final {
public:
  // Starts the native wxWidgets application runtime for this GUI test.
  AppScope() {
    int argc = 0;
    char **argv = nullptr;
    started_ = wxEntryStart(argc, argv);
    if (started_ && wxTheApp)
      initialized_ = wxTheApp->CallOnInit();
  }

  // Cleans up the native application runtime after bitmap resources are gone.
  ~AppScope() {
    if (started_)
      wxEntryCleanup();
  }

  // Reports whether the native GUI runtime initialized successfully.
  bool IsOk() const { return started_ && initialized_; }

private:
  bool started_ = false;
  bool initialized_ = false;
};

// Renders one SVG through the production cache and requires a valid result.
wxImage RenderSvg(GdtfResourceBitmapCache &cache, const std::string &entry,
                  const std::string &svg, const wxSize &target) {
  const auto result =
      cache.GetOrCreateSvg("source", entry, svg, target, *wxWHITE);
  assert(result.decoded && result.bitmap.IsOk());
  assert(result.bitmap.GetSize() == target);
  return result.bitmap.ConvertToImage();
}

// Real compound-path holes expose both checker colours, regardless of winding.
void CheckNativePreviewHoles() {
  wxBitmap bitmap(100, 100);
  wxMemoryDC dc(bitmap);
  symbol_preview::DrawTransparencyBackground(dc, wxRect(0, 0, 100, 100));
  {
    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
    assert(gc);
    auto path = gc->CreatePath();
    const std::vector<wxPoint2DDouble> outer = {{5, 5}, {95, 5}, {95, 95}, {5, 95}};
    const std::vector<wxPoint2DDouble> hole = {{20, 20}, {70, 20}, {70, 70}, {20, 70}};
    const auto transform = [](const auto &point) { return point; };
    symbol_preview::AppendPreviewRing(path, outer, transform);
    symbol_preview::AppendPreviewRing(path, hole, transform);
    gc->SetBrush(wxBrush(wxColour(224, 224, 224)));
    gc->FillPath(path, wxODDEVEN_RULE);
  }
  dc.SelectObject(wxNullBitmap);
  const auto image = bitmap.ConvertToImage();
  assert(PixelNear(image, 15, 15, 224, 224, 224));
  assert(PixelNear(image, 25, 25, 255, 255, 255));
  assert(PixelNear(image, 35, 25, 244, 244, 244));
}

} // namespace

// Verifies SVG fitting, alpha composition, authored white, and safe failures.
int main() {
  AppScope app;
  if (!app.IsOk())
    return 77;
  CheckNativePreviewHoles();
  GdtfResourceBitmapCache cache;

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

  symbols::Symbol2D holed;
  holed.bounds = {{0, 0}, {100, 100}, true};
  holed.fill = {{{{10, 10}, {90, 10}, {90, 90}, {10, 90}},
                 {{{30, 30}, {70, 30}, {70, 70}, {30, 70}}}}};
  holed.strokes = {{{10, 10}, {90, 10}}};
  std::string generated, error;
  assert(symbols::SerializeSymbolToSvg(holed, generated, error));
  const std::string oldSvg =
      "<svg xmlns='http://www.w3.org/2000/svg' version='1.1' viewBox='0 0 100 100'>"
      "<polygon points='10,90 90,90 90,10 10,10' fill='#e0e0e0' stroke='none'/>"
      "<polygon points='30,70 70,70 70,30 30,30' fill='#ffffff' stroke='none'/>"
      "<polyline points='10,90 90,90' fill='none' stroke='#000000' stroke-width='2'/>"
      "</svg>";
  const wxImage generatedPreview =
      RenderSvg(cache, "generated-holes.svg", generated, wxSize(100, 100));
  const wxImage oldPreview =
      RenderSvg(cache, "legacy-holes.svg", oldSvg, wxSize(100, 100));
  assert(IsCheckerPixel(generatedPreview, 50, 50));
  assert(PixelNear(oldPreview, 50, 50, 255, 255, 255));
  assert(PixelNear(generatedPreview, 20, 20, 224, 224, 224));
  assert(PixelNear(generatedPreview, 50, 90, 0, 0, 0));

  // Compare real SVG rasterizations after composition on a white background.
  auto rasterize = [](const std::string &svg) {
    const auto bundle = wxBitmapBundle::FromSVG(svg.c_str(), wxSize(100, 100));
    assert(bundle.IsOk());
    return bundle.GetBitmap(wxSize(100, 100)).ConvertToImage();
  };
  const wxImage newRaster = rasterize(generated);
  const wxImage oldRaster = rasterize(oldSvg);
  assert(newRaster.HasAlpha() && newRaster.GetAlpha(50, 50) == 0);
  assert(oldRaster.HasAlpha() && oldRaster.GetAlpha(50, 50) == 255);
  for (int y = 0; y < 100; ++y) {
    for (int x = 0; x < 100; ++x) {
      auto onWhite = [&](const wxImage &image, unsigned char channel) {
        const int alpha = image.GetAlpha(x, y);
        return (channel * alpha + 255 * (255 - alpha) + 127) / 255;
      };
      assert(std::abs(onWhite(newRaster, newRaster.GetRed(x, y)) -
                      onWhite(oldRaster, oldRaster.GetRed(x, y))) <= 1);
      assert(std::abs(onWhite(newRaster, newRaster.GetGreen(x, y)) -
                      onWhite(oldRaster, oldRaster.GetGreen(x, y))) <= 1);
      assert(std::abs(onWhite(newRaster, newRaster.GetBlue(x, y)) -
                      onWhite(oldRaster, oldRaster.GetBlue(x, y))) <= 1);
    }
  }

  const auto invalid = cache.GetOrCreateSvg("source", "broken.svg", "not svg",
                                            wxSize(64, 48), *wxWHITE);
  assert(!invalid.decoded && invalid.bitmap.IsOk());
  const auto oversized = cache.GetOrCreateSvg("source", "large.svg", square,
                                              wxSize(5000, 48), *wxWHITE);
  assert(!oversized.decoded);
  assert(oversized.status == GdtfBitmapDecodeStatus::DimensionsTooLarge);
  return 0;
}
