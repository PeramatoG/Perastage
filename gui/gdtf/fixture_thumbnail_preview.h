#pragma once

#include "gdtf/gdtf_resource_bitmap_cache.h"

#include <string>
#include <vector>

struct GdtfThumbnailPreviewOption {
  std::string archivePath;
  bool svg = false;
};

// Returns canonical authored thumbnail formats available for direct inspection.
std::vector<GdtfThumbnailPreviewOption> ListGdtfThumbnailPreviewOptions(
    const std::string &path);

// Uses Core's canonical/compatibility candidates and optionally selects one exact resource.
GdtfBitmapDecodeResult LoadGdtfThumbnailPreview(
    const std::string &path, const wxSize &size, const wxColour &background,
    const std::string &preferredArchivePath = {});
