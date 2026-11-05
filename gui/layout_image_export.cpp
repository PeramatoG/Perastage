#include "layout_image_export.h"
#include <wx/image.h>
#include <algorithm>
#include <cmath>
namespace gui {
wxSize LayoutImageRasterSize(const layouts::Layout2DViewFrame &frame,
    layouts::composition::PageOutputMapping mapping) {
  if (frame.width <= 0 || frame.height <= 0 || !std::isfinite(mapping.scale) || mapping.scale <= 0) return {};
  // Bound in floating point before conversion/allocation. One resolution factor
  // preserves frame aspect, with the existing 8192 side / 8192^2 pixel caps.
  const double pixelsPerPoint = std::min({mapping.scale * (300.0/72.0),
      8192.0/frame.width, 8192.0/frame.height});
  return {std::clamp(static_cast<int>(std::lround(frame.width*pixelsPerPoint)),1,8192),
          std::clamp(static_cast<int>(std::lround(frame.height*pixelsPerPoint)),1,8192)};
}
std::optional<LayoutImageExportData> BuildLayoutImageExportData(
    const layouts::LayoutImageDefinition &imageDef, layouts::composition::PageOutputMapping mapping,
    std::string *diagnostic) {
  if (diagnostic) diagnostic->clear();
  auto fail = [&](const char *message) -> std::optional<LayoutImageExportData> {
    if (diagnostic) *diagnostic = message;
    return std::nullopt;
  };
  LayoutImageExportData data;
  data.zIndex = imageDef.zIndex;
  data.frame = imageDef.frame;
  if (data.frame.width <= 0 || data.frame.height <= 0)
    return fail("Layout image input is unavailable or invalid; image omitted.");
  if (imageDef.imagePath.empty())
    return fail("Layout image input is unavailable or invalid; image omitted.");

  wxImage sourceImage;
  if (!sourceImage.LoadFile(wxString::FromUTF8(imageDef.imagePath)))
    return fail("Layout image input is unavailable or invalid; image omitted.");
  if (sourceImage.GetWidth() <= 0 || sourceImage.GetHeight() <= 0)
    return fail("Layout image input is unavailable or invalid; image omitted.");

  const wxSize target = LayoutImageRasterSize(data.frame, mapping);
  if (target.x <= 0 || target.y <= 0) return fail("Invalid output image raster size.");
  const int targetPixelWidth = target.x, targetPixelHeight = target.y;

  wxImage scaled =
      sourceImage.Scale(targetPixelWidth, targetPixelHeight, wxIMAGE_QUALITY_HIGH);
  if (!scaled.IsOk() || scaled.GetWidth() <= 0 || scaled.GetHeight() <= 0)
    return fail("Layout image input is unavailable or invalid; image omitted.");

  data.pixelWidth = scaled.GetWidth();
  data.pixelHeight = scaled.GetHeight();
  data.pixels.resize(static_cast<size_t>(data.pixelWidth) *
                     static_cast<size_t>(data.pixelHeight) * 3u);

  const unsigned char *rgb = scaled.GetData();
  const unsigned char *alpha = scaled.HasAlpha() ? scaled.GetAlpha() : nullptr;
  if (!rgb)
    return fail("Layout image input is unavailable or invalid; image omitted.");
  for (int idx = 0; idx < data.pixelWidth * data.pixelHeight; ++idx) {
    const size_t rgbOffset = static_cast<size_t>(idx) * 3u;
    if (!alpha) {
      data.pixels[rgbOffset + 0] = rgb[rgbOffset + 0];
      data.pixels[rgbOffset + 1] = rgb[rgbOffset + 1];
      data.pixels[rgbOffset + 2] = rgb[rgbOffset + 2];
      continue;
    }

    const double a = static_cast<double>(alpha[idx]) / 255.0;
    data.pixels[rgbOffset + 0] = static_cast<unsigned char>(
        std::clamp((static_cast<double>(rgb[rgbOffset + 0]) * a) +
                       (255.0 * (1.0 - a)),
                   0.0, 255.0));
    data.pixels[rgbOffset + 1] = static_cast<unsigned char>(
        std::clamp((static_cast<double>(rgb[rgbOffset + 1]) * a) +
                       (255.0 * (1.0 - a)),
                   0.0, 255.0));
    data.pixels[rgbOffset + 2] = static_cast<unsigned char>(
        std::clamp((static_cast<double>(rgb[rgbOffset + 2]) * a) +
                       (255.0 * (1.0 - a)),
                   0.0, 255.0));
  }
  return data;
}
}
