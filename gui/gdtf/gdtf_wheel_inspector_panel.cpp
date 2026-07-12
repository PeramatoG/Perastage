#include "gdtf_wheel_inspector_panel.h"

#include <algorithm>
#include <sstream>

#include <wx/brush.h>
#include <wx/dcmemory.h>
#include <wx/imaglist.h>
#include <wx/listctrl.h>
#include <wx/event.h>
#include <wx/scrolwin.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/statbmp.h>
#include <wx/stattext.h>

namespace {
constexpr int kSlotThumbnailSize = 48;
constexpr int kActivePreviewSize = 180;
}

// Creates the read-only wheel and slot inspector panel.
GdtfWheelInspectorPanel::GdtfWheelInspectorPanel(wxWindow *parent)
    : wxPanel(parent, wxID_ANY) {
  auto *root = new wxBoxSizer(wxVERTICAL);
  root->Add(new wxStaticText(this, wxID_ANY, "DMX inspection details"), 0, wxBOTTOM, 3);
  activeDetailsPanel = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition,
                                            wxDefaultSize, wxBORDER_SIMPLE | wxVSCROLL);
  activeDetailsPanel->SetScrollRate(0, 8);
  activeDetailsSizer = new wxBoxSizer(wxVERTICAL);
  activeDetailsPanel->SetSizer(activeDetailsSizer);
  activeDetailsPanel->Bind(wxEVT_SIZE, [this](wxSizeEvent &event) {
    ApplyDetailRows(activeDetailsPanel, activeDetailsSizer, activeDetailRows);
    event.Skip();
  });
  root->Add(activeDetailsPanel, 1, wxEXPAND | wxBOTTOM, 6);

  auto *previewRow = new wxBoxSizer(wxHORIZONTAL);
  activePreviewBitmap = new wxStaticBitmap(this, wxID_ANY,
                                           CreatePlaceholderBitmap(wxSize(kActivePreviewSize, kActivePreviewSize)));
  previewRow->Add(activePreviewBitmap, 0, wxRIGHT, 6);
  previewDetailsPanel = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition,
                                             wxDefaultSize, wxBORDER_SIMPLE | wxVSCROLL);
  previewDetailsPanel->SetScrollRate(0, 8);
  previewDetailsSizer = new wxBoxSizer(wxVERTICAL);
  previewDetailsPanel->SetSizer(previewDetailsSizer);
  previewDetailsPanel->Bind(wxEVT_SIZE, [this](wxSizeEvent &event) {
    ApplyDetailRows(previewDetailsPanel, previewDetailsSizer, previewDetailRows);
    event.Skip();
  });
  previewRow->Add(previewDetailsPanel, 1, wxEXPAND);
  root->Add(previewRow, 0, wxEXPAND | wxBOTTOM, 6);

  root->Add(new wxStaticText(this, wxID_ANY, "Wheel slots"), 0, wxBOTTOM, 3);
  slotList = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                            wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_NO_HEADER);
  slotList->AppendColumn("Slot", wxLIST_FORMAT_LEFT, 720);
  slotImages = new wxImageList(kSlotThumbnailSize, kSlotThumbnailSize, true);
  slotList->AssignImageList(slotImages, wxIMAGE_LIST_SMALL);
  root->Add(slotList, 1, wxEXPAND);
  slotList->Bind(wxEVT_LIST_ITEM_SELECTED, &GdtfWheelInspectorPanel::OnSlotSelected, this);
  SetSizer(root);
  ClearPresentation();
}

// Applies the current read-only wheel inspection presentation.
void GdtfWheelInspectorPanel::SetPresentation(
    const GdtfWheelInspectorPresentation &presentation) {
  currentSlots = presentation.slots;
  SetDetailRows(activeDetailsPanel, activeDetailsSizer, activeDetailRows,
                presentation.detailRows.empty() ? BuildStatusRows(presentation.activeText)
                                                : presentation.detailRows);
  if (presentation.hasActivePreview)
    activePreviewBitmap->SetBitmap(presentation.activePreview);
  else if (presentation.hasActiveSwatch)
    activePreviewBitmap->SetBitmap(CreateSwatchBitmap(presentation.activeSwatch,
                                                      wxSize(kActivePreviewSize, kActivePreviewSize)));
  else
    activePreviewBitmap->SetBitmap(CreatePlaceholderBitmap(wxSize(kActivePreviewSize, kActivePreviewSize)));
  SetDetailRows(previewDetailsPanel, previewDetailsSizer, previewDetailRows,
                MergeDetailRows(presentation.previewRows, presentation.previewStatus));

  slotList->DeleteAllItems();
  slotImages->RemoveAll();
  long selectedIndex = -1;
  for (size_t i = 0; i < currentSlots.size(); ++i) {
    const auto &slot = currentSlots[i];
    wxBitmap image = slot.hasThumbnail ? slot.thumbnail
                     : slot.hasSwatch && slot.mediaResource.empty()
                         ? CreateSwatchBitmap(slot.swatch, wxSize(kSlotThumbnailSize, kSlotThumbnailSize))
                         : CreatePlaceholderBitmap(wxSize(kSlotThumbnailSize, kSlotThumbnailSize));
    const int imageIndex = slotImages->Add(image);
    const long row = slotList->InsertItem(static_cast<long>(i), wxString::FromUTF8(slot.label), imageIndex);
    if (slot.selected)
      selectedIndex = row;
  }
  if (selectedIndex >= 0)
    slotList->SetItemState(selectedIndex, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED,
                           wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
}

// Clears the wheel inspector to an unavailable read-only state.
void GdtfWheelInspectorPanel::ClearPresentation() {
  SetDetailRows(activeDetailsPanel, activeDetailsSizer, activeDetailRows,
                {{"Status", "Select a DMX channel and move the inspection slider to resolve the active function, set, wheel, and slot."}});
  SetDetailRows(previewDetailsPanel, previewDetailsSizer, previewDetailRows,
                {{"Preview", "No wheel slot preview is available yet."}});
  activePreviewBitmap->SetBitmap(CreatePlaceholderBitmap(wxSize(kActivePreviewSize, kActivePreviewSize)));
  slotList->DeleteAllItems();
  slotImages->RemoveAll();
  currentSlots.clear();
}

// Applies a clicked wheel-slot preview without changing the resolved DMX mapping.
void GdtfWheelInspectorPanel::ApplySlotPreview(const GdtfWheelInspectorSlotPresentation &slot) {
  if (slot.hasPreview)
    activePreviewBitmap->SetBitmap(slot.preview);
  else if (slot.hasThumbnail)
    activePreviewBitmap->SetBitmap(slot.thumbnail);
  else if (slot.hasSwatch && slot.mediaResource.empty())
    activePreviewBitmap->SetBitmap(CreateSwatchBitmap(slot.swatch, wxSize(kActivePreviewSize, kActivePreviewSize)));
  else
    activePreviewBitmap->SetBitmap(CreatePlaceholderBitmap(wxSize(kActivePreviewSize, kActivePreviewSize)));

  SetDetailRows(previewDetailsPanel, previewDetailsSizer, previewDetailRows,
                MergeDetailRows(slot.detailRows, slot.previewStatus));
}

// Updates the preview pane when the user selects a wheel slot row.
void GdtfWheelInspectorPanel::OnSlotSelected(wxListEvent &event) {
  const long row = event.GetIndex();
  if (row < 0 || static_cast<size_t>(row) >= currentSlots.size())
    return;
  ApplySlotPreview(currentSlots[static_cast<size_t>(row)]);
}

// Stores and applies key/value detail rows to a wrapping details panel.
void GdtfWheelInspectorPanel::SetDetailRows(
    wxScrolledWindow *panel, wxBoxSizer *sizer,
    std::vector<GdtfWheelInspectorDetailRow> &storedRows,
    const std::vector<GdtfWheelInspectorDetailRow> &rows) {
  storedRows = rows;
  ApplyDetailRows(panel, sizer, storedRows);
}

// Applies key/value detail rows with wrapped value text.
void GdtfWheelInspectorPanel::ApplyDetailRows(
    wxScrolledWindow *panel, wxBoxSizer *sizer,
    const std::vector<GdtfWheelInspectorDetailRow> &rows) {
  if (!panel || !sizer)
    return;
  sizer->Clear(true);
  const int labelWidth = 118;
  const int wrapWidth = std::max(160, panel->GetClientSize().GetWidth() - labelWidth - 18);
  for (const auto &detail : rows) {
    auto *rowSizer = new wxBoxSizer(wxHORIZONTAL);
    auto *label = new wxStaticText(panel, wxID_ANY, wxString::FromUTF8(detail.label));
    label->SetMinSize(wxSize(labelWidth, -1));
    auto labelFont = label->GetFont();
    labelFont.SetWeight(wxFONTWEIGHT_BOLD);
    label->SetFont(labelFont);
    auto *value = new wxStaticText(panel, wxID_ANY, wxString::FromUTF8(detail.value));
    value->Wrap(wrapWidth);
    rowSizer->Add(label, 0, wxRIGHT | wxBOTTOM, 8);
    rowSizer->Add(value, 1, wxEXPAND | wxBOTTOM, 8);
    sizer->Add(rowSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 6);
  }
  panel->FitInside();
  panel->Layout();
}

// Converts a multi-line status string into displayable detail rows.
std::vector<GdtfWheelInspectorDetailRow> GdtfWheelInspectorPanel::BuildStatusRows(
    const std::string &status) const {
  std::vector<GdtfWheelInspectorDetailRow> rows;
  std::stringstream input(status);
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty())
      continue;
    const auto separator = line.find(':');
    if (separator == std::string::npos)
      rows.push_back({"Info", line});
    else
      rows.push_back({line.substr(0, separator), line.substr(separator + 1)});
  }
  if (rows.empty())
    rows.push_back({"Info", "-"});
  return rows;
}

// Combines structured detail rows with optional status diagnostics.
std::vector<GdtfWheelInspectorDetailRow> GdtfWheelInspectorPanel::MergeDetailRows(
    const std::vector<GdtfWheelInspectorDetailRow> &details,
    const std::string &status) const {
  std::vector<GdtfWheelInspectorDetailRow> rows = details;
  const auto statusRows = BuildStatusRows(status);
  if (!status.empty())
    rows.insert(rows.end(), statusRows.begin(), statusRows.end());
  if (rows.empty())
    rows.push_back({"Info", "-"});
  return rows;
}

// Creates a bitmap filled with the approximate display color for one slot.
wxBitmap GdtfWheelInspectorPanel::CreateSwatchBitmap(const wxColour &colour,
                                                     const wxSize &size) const {
  wxBitmap bitmap(std::max(1, size.GetWidth()), std::max(1, size.GetHeight()));
  wxMemoryDC dc(bitmap);
  dc.SetBackground(wxBrush(colour));
  dc.Clear();
  dc.SelectObject(wxNullBitmap);
  return bitmap;
}

// Creates a themed placeholder bitmap for slots without visual media.
wxBitmap GdtfWheelInspectorPanel::CreatePlaceholderBitmap(const wxSize &size) const {
  return CreateSwatchBitmap(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE), size);
}
