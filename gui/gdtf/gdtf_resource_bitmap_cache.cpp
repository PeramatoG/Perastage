#include "gdtf_resource_bitmap_cache.h"

#include <algorithm>
#include <sstream>

#include <wx/dcmemory.h>
#include <wx/brush.h>
#include <wx/image.h>
#include <wx/mstream.h>

// Creates a bounded GUI-only bitmap cache for GDTF resource previews.
GdtfResourceBitmapCache::GdtfResourceBitmapCache(std::size_t maxBytes)
    : maxCacheBytes(maxBytes) {}

// Clears all decoded preview bitmaps.
void GdtfResourceBitmapCache::Clear() {
  entries.clear();
  currentBytes = 0;
}

// Returns a decoded and scaled bitmap or a safe themed placeholder.
wxBitmap GdtfResourceBitmapCache::GetOrCreate(const std::string &sourceId,
                                              const std::string &entryPath,
                                              const std::vector<unsigned char> &bytes,
                                              const wxSize &targetSize,
                                              const wxColour &placeholderColor) {
  const std::string key = MakeKey(sourceId, entryPath, targetSize);
  auto it = entries.find(key);
  if (it != entries.end())
    return it->second.bitmap;
  wxBitmap bitmap = MakePlaceholder(targetSize, placeholderColor);
  if (!bytes.empty() && targetSize.GetWidth() > 0 && targetSize.GetHeight() > 0) {
    wxMemoryInputStream stream(bytes.data(), bytes.size());
    wxImage image(stream, wxBITMAP_TYPE_ANY);
    if (image.IsOk() && image.GetWidth() > 0 && image.GetHeight() > 0 &&
        image.GetWidth() <= 4096 && image.GetHeight() <= 4096) {
      image.Rescale(targetSize.GetWidth(), targetSize.GetHeight(), wxIMAGE_QUALITY_HIGH);
      bitmap = wxBitmap(image);
    }
  }
  const std::size_t estimate = static_cast<std::size_t>(std::max(1, targetSize.GetWidth())) *
                               static_cast<std::size_t>(std::max(1, targetSize.GetHeight())) * 4u;
  entries[key] = {bitmap, estimate};
  currentBytes += estimate;
  EnforceLimit();
  return bitmap;
}

// Builds a stable cache key for one source, entry, and target size.
std::string GdtfResourceBitmapCache::MakeKey(const std::string &sourceId,
                                             const std::string &entryPath,
                                             const wxSize &targetSize) const {
  std::ostringstream key;
  key << sourceId << '\n' << entryPath << '\n' << targetSize.GetWidth() << 'x' << targetSize.GetHeight();
  return key.str();
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

// Drops oldest keyed entries until the approximate memory limit is respected.
void GdtfResourceBitmapCache::EnforceLimit() {
  while (currentBytes > maxCacheBytes && !entries.empty()) {
    auto it = entries.begin();
    currentBytes -= std::min(currentBytes, it->second.bytes);
    entries.erase(it);
  }
}
