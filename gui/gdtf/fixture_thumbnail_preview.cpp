#include "gdtf/fixture_thumbnail_preview.h"

#include "gdtf_archive_reader.h"
#include "gdtf_thumbnail.h"
#include "filesystem_path_utils.h"

#include <algorithm>
#include <utility>
#include <vector>

#include <wx/dcmemory.h>
#include <wx/settings.h>

namespace {

struct DecodedThumbnail {
  gdtf::ThumbnailResource resource;
  GdtfBitmapDecodeResult result;
};

// Adds resource provenance to a decoded thumbnail diagnostic.
GdtfBitmapDecodeResult DecodeThumbnailResource(
    GdtfResourceBitmapCache &cache, const std::string &path,
    const std::filesystem::path &source,
    const gdtf::ThumbnailResource &resource, const wxSize &size,
    const wxColour &background) {
  GdtfBitmapDecodeResult result;
  const auto bytes =
      gdtf::ReadGdtfArchiveResource(source, resource.archivePath);
  if (!bytes.Success() || bytes.filesystemFallback ||
      bytes.entryPath != resource.archivePath)
    return result;

  result = resource.svg
               ? cache.GetOrCreateSvg(
                     path, resource.archivePath,
                     std::string(bytes.bytes.begin(), bytes.bytes.end()), size,
                     background)
               : cache.GetOrCreate(path, resource.archivePath, bytes.bytes, size,
                                   background);
  result.diagnostic =
      resource.archivePath +
      (resource.compatibilityFallback ? " (compatibility import): "
                                      : " (canonical): ") +
      result.diagnostic;
  return result;
}

// Returns the per-resource target size used by a two-up canonical preview.
wxSize CanonicalPairPreviewSize(const wxSize &targetSize) {
  constexpr int kOuterPadding = 6;
  constexpr int kGap = 8;
  constexpr int kLabelHeight = 22;
  const int width = std::max(1, targetSize.GetWidth());
  const int height = std::max(1, targetSize.GetHeight());
  const int paneWidth = std::max(1, (width - (2 * kOuterPadding) - kGap) / 2);
  const int paneHeight =
      std::max(1, height - kLabelHeight - (2 * kOuterPadding));
  return wxSize(paneWidth, paneHeight);
}

// Composes the canonical SVG and PNG previews without hiding either resource.
wxBitmap ComposeCanonicalThumbnailPair(
    const std::vector<DecodedThumbnail> &previews, const wxSize &targetSize,
    const wxColour &background) {
  constexpr int kOuterPadding = 6;
  constexpr int kGap = 8;
  constexpr int kLabelHeight = 22;
  const int width = std::max(1, targetSize.GetWidth());
  const int height = std::max(1, targetSize.GetHeight());
  const int paneWidth = std::max(1, (width - (2 * kOuterPadding) - kGap) / 2);

  wxBitmap composite(width, height);
  wxMemoryDC dc(composite);
  dc.SetBackground(wxBrush(background));
  dc.Clear();
  dc.SetTextForeground(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));

  for (std::size_t index = 0; index < previews.size() && index < 2; ++index) {
    const auto &preview = previews[index];
    const int x = kOuterPadding +
                  static_cast<int>(index) * (paneWidth + kGap);
    const wxString label = preview.resource.svg ? "SVG" : "PNG";
    wxCoord textWidth = 0;
    wxCoord textHeight = 0;
    dc.GetTextExtent(label, &textWidth, &textHeight);
    dc.DrawText(label, x + std::max(0, (paneWidth - textWidth) / 2),
                std::max(0, (kLabelHeight - textHeight) / 2));
    dc.DrawBitmap(preview.result.bitmap, x, kLabelHeight + kOuterPadding, true);
  }

  dc.SelectObject(wxNullBitmap);
  return composite;
}

} // namespace

GdtfBitmapDecodeResult LoadGdtfThumbnailPreview(
    const std::string &path, const wxSize &size, const wxColour &background) {
  GdtfResourceBitmapCache cache;
  GdtfBitmapDecodeResult result;
  const auto source = PathUtils::PathFromUtf8(path);
  const auto resources = gdtf::InspectGdtfThumbnails(source);
  const std::size_t canonicalCount = static_cast<std::size_t>(std::count_if(
      resources.begin(), resources.end(), [](const auto &resource) {
        return !resource.compatibilityFallback;
      }));

  if (canonicalCount >= 2) {
    const wxSize pairSize = CanonicalPairPreviewSize(size);
    std::vector<DecodedThumbnail> decoded;
    decoded.reserve(canonicalCount);
    for (const auto &resource : resources) {
      if (resource.compatibilityFallback)
        continue;
      auto candidate = DecodeThumbnailResource(cache, path, source, resource,
                                               pairSize, background);
      result = candidate;
      if (candidate.decoded)
        decoded.push_back({resource, std::move(candidate)});
    }

    if (decoded.size() >= 2) {
      result = decoded.front().result;
      result.bitmap = ComposeCanonicalThumbnailPair(decoded, size, background);
      result.diagnostic = decoded.front().result.diagnostic + " | " +
                          decoded[1].result.diagnostic;
      return result;
    }

    if (decoded.size() == 1) {
      return DecodeThumbnailResource(cache, path, source, decoded.front().resource,
                                     size, background);
    }

    return result;
  }

  for (const auto &resource : resources) {
    result = DecodeThumbnailResource(cache, path, source, resource, size,
                                     background);
    if (result.decoded)
      return result;
  }
  return result;
}
