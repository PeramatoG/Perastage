#pragma once

#include "gdtf/gdtf_resource_bitmap_cache.h"

#include <string>
#include <vector>

#include <wx/panel.h>

class wxChoice;
class wxStaticBitmap;

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

class FixtureThumbnailPreviewPanel : public wxPanel {
public:
  explicit FixtureThumbnailPreviewPanel(wxWindow *parent);

  // Rebinds the panel to one GDTF archive and refreshes available formats.
  void SetArchivePath(const std::string &path);

private:
  void RebuildOptions();
  void RefreshPreview();

  wxChoice *formatSelector = nullptr;
  wxStaticBitmap *bitmap = nullptr;
  std::string archivePath;
  std::vector<GdtfThumbnailPreviewOption> options;
};
