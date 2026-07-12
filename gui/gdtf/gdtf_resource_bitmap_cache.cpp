#include "gdtf_resource_bitmap_cache.h"

#include <algorithm>
#include <sstream>

#include <wx/brush.h>
#include <wx/dcmemory.h>
#include <wx/image.h>
#include <wx/init.h>
#include <wx/mstream.h>
#include <wx/pen.h>

namespace {
constexpr int kMaxPreviewDimension = 4096;
constexpr int kCheckerSize = 8;

// Ensures wx image handlers are available before decoding archive PNG/JPEG resources.
void EnsureImageHandlersInitialized() {
  static const bool initialized = []() {
    wxInitAllImageHandlers();
    return true;
  }();
  (void)initialized;
}

// Returns a readable diagnostic for one decode status.
std::string DecodeStatusText(GdtfBitmapDecodeStatus status) {
  switch (status) {
  case GdtfBitmapDecodeStatus::Success:
    return "Decoded image successfully.";
  case GdtfBitmapDecodeStatus::EmptyResourceData:
    return "Resource data is empty.";
  case GdtfBitmapDecodeStatus::UnsupportedImage:
    return "Image format is unsupported or unrecognized.";
  case GdtfBitmapDecodeStatus::DecodeFailure:
    return "Image decoding failed.";
  case GdtfBitmapDecodeStatus::InvalidDimensions:
    return "Decoded image dimensions are invalid.";
  case GdtfBitmapDecodeStatus::DimensionsTooLarge:
    return "Decoded image dimensions exceed the safety limit.";
  }
  return "Image decoding failed.";
}
} // namespace

// Creates a bounded GUI-only bitmap cache for GDTF resource previews.
GdtfResourceBitmapCache::GdtfResourceBitmapCache(std::size_t maxBytes)
    : maxCacheBytes(maxBytes) {}

// Clears all decoded preview bitmaps.
void GdtfResourceBitmapCache::Clear() {
  entries.clear();
  currentBytes = 0;
}

// Returns an explicit decode result with a bitmap suitable for display.
GdtfBitmapDecodeResult GdtfResourceBitmapCache::GetOrCreate(
    const std::string &sourceFingerprint, const std::string &entryPath,
    const std::vector<unsigned char> &bytes, const wxSize &targetSize,
    const wxColour &placeholderColor) {
  EnsureImageHandlersInitialized();
  const std::string key = MakeKey(sourceFingerprint, entryPath, targetSize);
  auto it = entries.find(key);
  if (it != entries.end())
    return it->second.result;

  GdtfBitmapDecodeResult result = DecodeResource(bytes, targetSize, placeholderColor);
  const std::size_t estimate = static_cast<std::size_t>(std::max(1, targetSize.GetWidth())) *
                               static_cast<std::size_t>(std::max(1, targetSize.GetHeight())) * 4u;
  entries[key] = {result, estimate};
  currentBytes += estimate;
  EnforceLimit();
  return result;
}

// Builds a stable cache key for one source fingerprint, entry, and target size.
std::string GdtfResourceBitmapCache::MakeKey(const std::string &sourceFingerprint,
                                             const std::string &entryPath,
                                             const wxSize &targetSize) const {
  std::ostringstream key;
  key << sourceFingerprint << '\n' << entryPath << '\n' << targetSize.GetWidth()
      << 'x' << targetSize.GetHeight();
  return key.str();
}

// Composes a decoded image over a checkerboard background using aspect fit.
wxBitmap GdtfResourceBitmapCache::ComposePreviewBitmap(const wxImage &image,
                                                       const wxSize &targetSize) const {
  const int targetWidth = std::max(1, targetSize.GetWidth());
  const int targetHeight = std::max(1, targetSize.GetHeight());
  wxBitmap composed(targetWidth, targetHeight, 32);
  wxMemoryDC dc(composed);
  const wxColour light(210, 210, 210);
  const wxColour dark(145, 145, 145);
  for (int y = 0; y < targetHeight; y += kCheckerSize) {
    for (int x = 0; x < targetWidth; x += kCheckerSize) {
      const bool lightSquare = ((x / kCheckerSize) + (y / kCheckerSize)) % 2 == 0;
      dc.SetBrush(wxBrush(lightSquare ? light : dark));
      dc.SetPen(*wxTRANSPARENT_PEN);
      dc.DrawRectangle(x, y, kCheckerSize, kCheckerSize);
    }
  }

  const double scale = std::min(static_cast<double>(targetWidth) / image.GetWidth(),
                                static_cast<double>(targetHeight) / image.GetHeight());
  const int scaledWidth = std::max(1, static_cast<int>(image.GetWidth() * scale));
  const int scaledHeight = std::max(1, static_cast<int>(image.GetHeight() * scale));
  wxImage scaled = image.Copy();
  scaled.Rescale(scaledWidth, scaledHeight, wxIMAGE_QUALITY_HIGH);
  const wxBitmap scaledBitmap(scaled);
  dc.DrawBitmap(scaledBitmap, (targetWidth - scaledWidth) / 2,
                (targetHeight - scaledHeight) / 2, true);
  dc.SelectObject(wxNullBitmap);
  return composed;
}

// Creates a plain placeholder bitmap using the system theme color supplied by the caller.
wxBitmap GdtfResourceBitmapCache::MakePlaceholder(const wxSize &targetSize,
                                                  const wxColour &color) const {
  wxBitmap bitmap(std::max(1, targetSize.GetWidth()), std::max(1, targetSize.GetHeight()), 32);
  wxMemoryDC dc(bitmap);
  dc.SetBackground(wxBrush(color));
  dc.Clear();
  dc.SelectObject(wxNullBitmap);
  return bitmap;
}

// Decodes raw image bytes and reports success separately from placeholders.
GdtfBitmapDecodeResult GdtfResourceBitmapCache::DecodeResource(
    const std::vector<unsigned char> &bytes, const wxSize &targetSize,
    const wxColour &placeholderColor) const {
  GdtfBitmapDecodeResult result;
  result.bitmap = MakePlaceholder(targetSize, placeholderColor);
  if (bytes.empty()) {
    result.status = GdtfBitmapDecodeStatus::EmptyResourceData;
    result.diagnostic = DecodeStatusText(result.status);
    return result;
  }
  if (targetSize.GetWidth() <= 0 || targetSize.GetHeight() <= 0) {
    result.status = GdtfBitmapDecodeStatus::InvalidDimensions;
    result.diagnostic = DecodeStatusText(result.status);
    return result;
  }

  wxMemoryInputStream stream(bytes.data(), bytes.size());
  wxImage image(stream, wxBITMAP_TYPE_ANY);
  if (!image.IsOk()) {
    result.status = GdtfBitmapDecodeStatus::UnsupportedImage;
    result.diagnostic = DecodeStatusText(result.status);
    return result;
  }
  result.sourceWidth = image.GetWidth();
  result.sourceHeight = image.GetHeight();
  if (result.sourceWidth <= 0 || result.sourceHeight <= 0) {
    result.status = GdtfBitmapDecodeStatus::InvalidDimensions;
    result.diagnostic = DecodeStatusText(result.status);
    return result;
  }
  if (result.sourceWidth > kMaxPreviewDimension || result.sourceHeight > kMaxPreviewDimension) {
    result.status = GdtfBitmapDecodeStatus::DimensionsTooLarge;
    result.diagnostic = DecodeStatusText(result.status);
    return result;
  }

  result.bitmap = ComposePreviewBitmap(image, targetSize);
  result.status = GdtfBitmapDecodeStatus::Success;
  result.decoded = true;
  result.diagnostic = DecodeStatusText(result.status);
  return result;
}

// Drops oldest keyed entries until the approximate memory limit is respected.
void GdtfResourceBitmapCache::EnforceLimit() {
  while (currentBytes > maxCacheBytes && !entries.empty()) {
    auto it = entries.begin();
    currentBytes -= std::min(currentBytes, it->second.bytes);
    entries.erase(it);
  }
}
