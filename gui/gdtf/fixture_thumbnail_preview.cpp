#include "gdtf/fixture_thumbnail_preview.h"

#include "gdtf_archive_reader.h"
#include "gdtf_thumbnail.h"
#include "filesystem_path_utils.h"

GdtfBitmapDecodeResult LoadGdtfThumbnailPreview(
    const std::string &path, const wxSize &size, const wxColour &background) {
  GdtfResourceBitmapCache cache;
  GdtfBitmapDecodeResult result;
  const auto source = PathUtils::PathFromUtf8(path);
  for (const auto &resource : gdtf::InspectGdtfThumbnails(source)) {
    const auto bytes = gdtf::ReadGdtfArchiveResource(source, resource.archivePath);
    if (!bytes.Success() || bytes.filesystemFallback || bytes.entryPath != resource.archivePath)
      continue;
    result = resource.svg
        ? cache.GetOrCreateSvg(path, resource.archivePath,
              std::string(bytes.bytes.begin(), bytes.bytes.end()), size, background)
        : cache.GetOrCreate(path, resource.archivePath, bytes.bytes, size, background);
    result.diagnostic = resource.archivePath +
        (resource.compatibilityFallback ? " (compatibility import): " : " (canonical): ") +
        result.diagnostic;
    if (result.decoded)
      return result;
  }
  return result;
}
