#pragma once

#include "gdtf/gdtf_resource_bitmap_cache.h"

// Uses Core's ordered canonical/compatibility candidates and shared archive reads.
GdtfBitmapDecodeResult LoadGdtfThumbnailPreview(
    const std::string &path, const wxSize &size, const wxColour &background);
