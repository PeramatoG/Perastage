#include "gdtf/fixture_thumbnail_preview.h"

#include "gdtf_archive_reader.h"
#include "gdtf_thumbnail.h"
#include "filesystem_path_utils.h"

#include <algorithm>

std::vector<GdtfThumbnailPreviewOption> ListGdtfThumbnailPreviewOptions(
    const std::string &path) {
  std::vector<GdtfThumbnailPreviewOption> result;
  const auto source = PathUtils::PathFromUtf8(path);
  for (const auto &resource : gdtf::InspectGdtfThumbnails(source)) {
    if (!resource.compatibilityFallback)
      result.push_back({resource.archivePath, resource.svg});
  }
  return result;
}

GdtfBitmapDecodeResult LoadGdtfThumbnailPreview(
    const std::string &path, const wxSize &size, const wxColour &background,
    const std::string &preferredArchivePath) {
  GdtfResourceBitmapCache cache;
  GdtfBitmapDecodeResult result;
  const auto source = PathUtils::PathFromUtf8(path);
  const auto resources = gdtf::InspectGdtfThumbnails(source);

  auto decode = [&](const gdtf::ThumbnailResource &resource) {
    const auto bytes = gdtf::ReadGdtfArchiveResource(source, resource.archivePath);
    if (!bytes.Success() || bytes.filesystemFallback ||
        bytes.entryPath != resource.archivePath)
      return GdtfBitmapDecodeResult{};
    auto decoded = resource.svg
        ? cache.GetOrCreateSvg(path, resource.archivePath,
              std::string(bytes.bytes.begin(), bytes.bytes.end()), size, background)
        : cache.GetOrCreate(path, resource.archivePath, bytes.bytes, size, background);
    decoded.diagnostic = resource.archivePath +
        (resource.compatibilityFallback ? " (compatibility import): " : " (canonical): ") +
        decoded.diagnostic;
    return decoded;
  };

  if (!preferredArchivePath.empty()) {
    const auto selected = std::find_if(
        resources.begin(), resources.end(), [&](const auto &resource) {
          return resource.archivePath == preferredArchivePath &&
                 !resource.compatibilityFallback;
        });
    if (selected != resources.end())
      return decode(*selected);
  }

  for (const auto &resource : resources) {
    result = decode(resource);
    if (result.decoded)
      return result;
  }
  return result;
}
