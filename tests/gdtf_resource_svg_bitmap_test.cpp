#include "gdtf/gdtf_resource_bitmap_cache.h"

#include <cassert>

#include <wx/init.h>

// Verifies SVG rendering succeeds in memory and reports malformed input safely.
int main() {
  wxInitializer initializer;
  if (!initializer.IsOk())
    return 77;
  GdtfResourceBitmapCache cache;
  const std::string svg =
      "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 10 10'>"
      "<rect width='10' height='10' fill='red'/></svg>";
  const auto valid = cache.GetOrCreateSvg("source", "icon.SVG", svg,
                                          wxSize(64, 48), *wxWHITE);
  assert(valid.decoded && valid.bitmap.IsOk());
  assert(valid.bitmap.GetWidth() <= 64 && valid.bitmap.GetHeight() <= 48);
  const auto invalid = cache.GetOrCreateSvg("source", "broken.svg", "not svg",
                                            wxSize(64, 48), *wxWHITE);
  assert(!invalid.decoded && invalid.bitmap.IsOk());
  const auto oversized = cache.GetOrCreateSvg("source", "large.svg", svg,
                                              wxSize(5000, 48), *wxWHITE);
  assert(!oversized.decoded);
  assert(oversized.status == GdtfBitmapDecodeStatus::DimensionsTooLarge);
  return 0;
}
