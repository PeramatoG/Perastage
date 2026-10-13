#include "gdtf/fixture_thumbnail_preview.h"

#include "filesystem_path_utils.h"
#include "gdtf_archive_reader.h"
#include "gdtf_thumbnail.h"

#include <algorithm>

#include <wx/choice.h>
#include <wx/dcmemory.h>
#include <wx/intl.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/statbmp.h>
#include <wx/stattext.h>

namespace {

// Resolves the themed background color used by preview placeholders and margins.
wxColour ResolvePreviewBackground(wxWindow *window) {
  for (wxWindow *current = window; current; current = current->GetParent()) {
    const wxColour colour = current->GetBackgroundColour();
    if (colour.IsOk())
      return colour;
  }
  return wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
}

// Creates a themed placeholder bitmap with centered text.
wxBitmap CreatePreviewPlaceholder(const wxString &label,
                                  const wxColour &background) {
  constexpr int kPreviewSize = 220;
  wxBitmap fallback(kPreviewSize, kPreviewSize);
  wxMemoryDC dc(fallback);
  dc.SetBackground(wxBrush(background));
  dc.Clear();
  dc.SetTextForeground(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
  dc.DrawLabel(label, wxRect(0, 0, kPreviewSize, kPreviewSize), wxALIGN_CENTER);
  dc.SelectObject(wxNullBitmap);
  return fallback;
}

} // namespace

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
    const auto bytes =
        gdtf::ReadGdtfArchiveResource(source, resource.archivePath);
    if (!bytes.Success() || bytes.filesystemFallback ||
        bytes.entryPath != resource.archivePath)
      return GdtfBitmapDecodeResult{};
    auto decoded = resource.svg
                       ? cache.GetOrCreateSvg(
                             path, resource.archivePath,
                             std::string(bytes.bytes.begin(), bytes.bytes.end()),
                             size, background)
                       : cache.GetOrCreate(path, resource.archivePath,
                                           bytes.bytes, size, background);
    decoded.diagnostic =
        resource.archivePath +
        (resource.compatibilityFallback ? " (compatibility import): "
                                        : " (canonical): ") +
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

FixtureThumbnailPreviewPanel::FixtureThumbnailPreviewPanel(wxWindow *parent)
    : wxPanel(parent, wxID_ANY) {
  auto *root = new wxBoxSizer(wxVERTICAL);
  auto *header = new wxBoxSizer(wxHORIZONTAL);
  header->Add(new wxStaticText(this, wxID_ANY, _("GDTF thumbnail")), 0,
              wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);

  formatSelector = new wxChoice(this, wxID_ANY);
  formatSelector->Hide();
  formatSelector->Bind(wxEVT_CHOICE,
                       [this](wxCommandEvent &) { RefreshPreview(); });
  header->Add(formatSelector, 0, wxALIGN_CENTER_VERTICAL);
  root->Add(header, 0, wxALIGN_CENTER_HORIZONTAL | wxTOP, 3);

  bitmap = new wxStaticBitmap(this, wxID_ANY, wxBitmap(220, 220));
  root->Add(bitmap, 1, wxALIGN_CENTER | wxALL, 8);
  SetSizer(root);
}

void FixtureThumbnailPreviewPanel::SetArchivePath(const std::string &path) {
  const bool sourceChanged = archivePath != path;
  archivePath = path;
  if (sourceChanged)
    RebuildOptions();
  else if (options.empty())
    RebuildOptions();
  RefreshPreview();
}

void FixtureThumbnailPreviewPanel::RebuildOptions() {
  options = ListGdtfThumbnailPreviewOptions(archivePath);
  formatSelector->Freeze();
  formatSelector->Clear();
  for (const auto &option : options)
    formatSelector->Append(option.svg ? "SVG" : "PNG");

  if (options.size() > 1) {
    formatSelector->SetSelection(0);
    formatSelector->Show();
  } else {
    if (!options.empty())
      formatSelector->SetSelection(0);
    formatSelector->Hide();
  }
  formatSelector->Thaw();
  Layout();
}

void FixtureThumbnailPreviewPanel::RefreshPreview() {
  const wxColour background = ResolvePreviewBackground(this);
  std::string preferredArchivePath;
  const int selection = formatSelector->GetSelection();
  if (selection >= 0 &&
      static_cast<std::size_t>(selection) < options.size()) {
    preferredArchivePath = options[static_cast<std::size_t>(selection)].archivePath;
  }

  const auto thumbnail = LoadGdtfThumbnailPreview(
      archivePath, wxSize(220, 220), background, preferredArchivePath);
  if (thumbnail.decoded) {
    bitmap->SetBitmap(thumbnail.bitmap);
    bitmap->SetToolTip(wxString::FromUTF8(thumbnail.diagnostic));
  } else {
    bitmap->SetBitmap(CreatePreviewPlaceholder("No image", background));
    bitmap->SetToolTip(thumbnail.diagnostic.empty()
                           ? _("No thumbnail image found in this GDTF.")
                           : wxString::FromUTF8(thumbnail.diagnostic));
  }
  Layout();
}
