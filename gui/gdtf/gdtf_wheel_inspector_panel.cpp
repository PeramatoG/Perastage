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
#include <wx/font.h>

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
    RewrapDetailRows(activeDetailsPanel, activeDetailControls);
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
    RewrapDetailRows(previewDetailsPanel, previewDetailControls);
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
                activeDetailControls,
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
                previewDetailControls,
                MergeDetailRows(presentation.previewRows, presentation.previewStatus));

  highlightedSlotIndex = -1;
  for (size_t i = 0; i < currentSlots.size(); ++i) {
    if (currentSlots[i].selected) {
      highlightedSlotIndex = static_cast<long>(i);
      break;
    }
  }
  RefreshSlotList();
}

// Clears the wheel inspector to an unavailable read-only state.
void GdtfWheelInspectorPanel::ClearPresentation() {
  SetDetailRows(activeDetailsPanel, activeDetailsSizer, activeDetailRows,
                activeDetailControls,
                {{"Status", "Select a DMX channel and move the inspection slider to resolve the active function, set, wheel, and slot."}});
  SetDetailRows(previewDetailsPanel, previewDetailsSizer, previewDetailRows,
                previewDetailControls,
                {{"Preview", "No wheel slot preview is available yet."}});
  activePreviewBitmap->SetBitmap(CreatePlaceholderBitmap(wxSize(kActivePreviewSize, kActivePreviewSize)));
  slotList->DeleteAllItems();
  slotImages->RemoveAll();
  currentSlots.clear();
  highlightedSlotIndex = -1;
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
                previewDetailControls,
                MergeDetailRows(slot.detailRows, slot.previewStatus));
}

// Rebuilds the slot gallery while keeping selection emphasis on text only.
void GdtfWheelInspectorPanel::RefreshSlotList() {
  slotList->DeleteAllItems();
  slotImages->RemoveAll();
  const wxFont normalFont = slotList->GetFont();
  wxFont highlightedFont = normalFont;
  highlightedFont.SetWeight(wxFONTWEIGHT_BOLD);
  for (size_t i = 0; i < currentSlots.size(); ++i) {
    const auto &slot = currentSlots[i];
    wxBitmap image = slot.hasThumbnail ? slot.thumbnail
                     : slot.hasSwatch && slot.mediaResource.empty()
                         ? CreateSwatchBitmap(slot.swatch, wxSize(kSlotThumbnailSize, kSlotThumbnailSize))
                         : CreatePlaceholderBitmap(wxSize(kSlotThumbnailSize, kSlotThumbnailSize));
    const int imageIndex = slotImages->Add(image);
    const long row = slotList->InsertItem(static_cast<long>(i), wxString::FromUTF8(slot.label), imageIndex);
    slotList->SetItemFont(row, row == highlightedSlotIndex ? highlightedFont : normalFont);
  }
}

// Updates the selected slot emphasis without applying native row highlight colors.
void GdtfWheelInspectorPanel::UpdateSlotHighlight(long row) {
  highlightedSlotIndex = row;
  const wxFont normalFont = slotList->GetFont();
  wxFont highlightedFont = normalFont;
  highlightedFont.SetWeight(wxFONTWEIGHT_BOLD);
  for (long i = 0; i < static_cast<long>(currentSlots.size()); ++i) {
    slotList->SetItemFont(i, i == highlightedSlotIndex ? highlightedFont : normalFont);
    slotList->SetItemState(i, 0, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
  }
}

// Updates the preview pane when the user selects a wheel slot row.
void GdtfWheelInspectorPanel::OnSlotSelected(wxListEvent &event) {
  const long row = event.GetIndex();
  if (row < 0 || static_cast<size_t>(row) >= currentSlots.size())
    return;
  UpdateSlotHighlight(row);
  ApplySlotPreview(currentSlots[static_cast<size_t>(row)]);
}

// Stores detail rows and updates existing controls when row labels are unchanged.
void GdtfWheelInspectorPanel::SetDetailRows(
    wxScrolledWindow *panel, wxBoxSizer *sizer,
    std::vector<GdtfWheelInspectorDetailRow> &storedRows,
    std::vector<GdtfWheelInspectorDetailControls> &controls,
    const std::vector<GdtfWheelInspectorDetailRow> &rows) {
  const bool sameStructure =
      storedRows.size() == rows.size() &&
      std::equal(storedRows.begin(), storedRows.end(), rows.begin(),
                 [](const auto &lhs, const auto &rhs) { return lhs.label == rhs.label; });
  storedRows = rows;
  if (sameStructure)
    UpdateDetailRowValues(panel, controls, storedRows);
  else
    RebuildDetailRows(panel, sizer, controls, storedRows);
}

// Rebuilds detail row controls when the row structure changes.
void GdtfWheelInspectorPanel::RebuildDetailRows(
    wxScrolledWindow *panel, wxBoxSizer *sizer,
    std::vector<GdtfWheelInspectorDetailControls> &controls,
    const std::vector<GdtfWheelInspectorDetailRow> &rows) {
  if (!panel || !sizer)
    return;
  panel->Freeze();
  sizer->Clear(true);
  controls.clear();
  const int labelWidth = 118;
  for (const auto &detail : rows) {
    GdtfWheelInspectorDetailControls row;
    row.rowSizer = new wxBoxSizer(wxHORIZONTAL);
    row.label = new wxStaticText(panel, wxID_ANY, wxString::FromUTF8(detail.label));
    row.label->SetMinSize(wxSize(labelWidth, -1));
    auto labelFont = row.label->GetFont();
    labelFont.SetWeight(wxFONTWEIGHT_BOLD);
    row.label->SetFont(labelFont);
    row.value = new wxStaticText(panel, wxID_ANY, wxString::FromUTF8(detail.value));
    row.rowSizer->Add(row.label, 0, wxRIGHT | wxBOTTOM, 8);
    row.rowSizer->Add(row.value, 1, wxEXPAND | wxBOTTOM, 8);
    sizer->Add(row.rowSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 6);
    controls.push_back(row);
  }
  RewrapDetailRows(panel, controls);
  panel->FitInside();
  panel->Layout();
  panel->Thaw();
}

// Updates existing detail row values without recreating their labels.
void GdtfWheelInspectorPanel::UpdateDetailRowValues(
    wxScrolledWindow *panel,
    std::vector<GdtfWheelInspectorDetailControls> &controls,
    const std::vector<GdtfWheelInspectorDetailRow> &rows) {
  if (!panel)
    return;
  bool changed = false;
  for (size_t i = 0; i < rows.size() && i < controls.size(); ++i) {
    if (!controls[i].value)
      continue;
    const wxString value = wxString::FromUTF8(rows[i].value);
    if (controls[i].value->GetLabel() != value) {
      controls[i].value->SetLabel(value);
      changed = true;
    }
  }
  if (changed) {
    RewrapDetailRows(panel, controls);
    panel->FitInside();
    panel->Layout();
  }
}

// Re-applies wrapping widths without rebuilding detail row controls.
void GdtfWheelInspectorPanel::RewrapDetailRows(
    wxScrolledWindow *panel,
    const std::vector<GdtfWheelInspectorDetailControls> &controls) {
  if (!panel)
    return;
  const int labelWidth = 118;
  const int wrapWidth = std::max(160, panel->GetClientSize().GetWidth() - labelWidth - 18);
  for (const auto &control : controls) {
    if (control.value)
      control.value->Wrap(wrapWidth);
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
