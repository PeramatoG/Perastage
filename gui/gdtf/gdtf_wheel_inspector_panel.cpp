#include "gdtf_wheel_inspector_panel.h"

#include <algorithm>

#include <wx/brush.h>
#include <wx/dcmemory.h>
#include <wx/imaglist.h>
#include <wx/listctrl.h>
#include <wx/event.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/statbmp.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace {
constexpr int kSlotThumbnailSize = 48;
constexpr int kActivePreviewSize = 180;
}

// Creates the read-only wheel and slot inspector panel.
GdtfWheelInspectorPanel::GdtfWheelInspectorPanel(wxWindow *parent)
    : wxPanel(parent, wxID_ANY) {
  auto *root = new wxBoxSizer(wxVERTICAL);
  root->Add(new wxStaticText(this, wxID_ANY, "Active DMX mapping"), 0, wxBOTTOM, 3);
  activeTextCtrl = new wxTextCtrl(this, wxID_ANY, wxString(), wxDefaultPosition,
                                  wxDefaultSize,
                                  wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);
  root->Add(activeTextCtrl, 1, wxEXPAND | wxBOTTOM, 6);

  auto *previewRow = new wxBoxSizer(wxHORIZONTAL);
  activePreviewBitmap = new wxStaticBitmap(this, wxID_ANY,
                                           CreatePlaceholderBitmap(wxSize(kActivePreviewSize, kActivePreviewSize)));
  previewRow->Add(activePreviewBitmap, 0, wxRIGHT, 6);
  previewStatusCtrl = new wxTextCtrl(this, wxID_ANY, wxString(), wxDefaultPosition,
                                     wxDefaultSize,
                                     wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);
  previewRow->Add(previewStatusCtrl, 1, wxEXPAND);
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
  activeTextCtrl->SetValue(wxString::FromUTF8(presentation.activeText));
  if (presentation.hasActivePreview)
    activePreviewBitmap->SetBitmap(presentation.activePreview);
  else if (presentation.hasActiveSwatch)
    activePreviewBitmap->SetBitmap(CreateSwatchBitmap(presentation.activeSwatch,
                                                      wxSize(kActivePreviewSize, kActivePreviewSize)));
  else
    activePreviewBitmap->SetBitmap(CreatePlaceholderBitmap(wxSize(kActivePreviewSize, kActivePreviewSize)));
  previewStatusCtrl->SetValue(wxString::FromUTF8(presentation.previewStatus));

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
  activeTextCtrl->SetValue("Select a DMX channel and move the inspection slider to resolve the active function, set, wheel, and slot.");
  previewStatusCtrl->SetValue("No wheel slot preview is available yet.");
  activePreviewBitmap->SetBitmap(CreatePlaceholderBitmap(wxSize(kActivePreviewSize, kActivePreviewSize)));
  slotList->DeleteAllItems();
  slotImages->RemoveAll();
  currentSlots.clear();
}

// Applies a clicked wheel-slot preview without changing the resolved DMX mapping.
void GdtfWheelInspectorPanel::ApplySlotPreview(const GdtfWheelInspectorSlotPresentation &slot) {
  if (slot.hasThumbnail)
    activePreviewBitmap->SetBitmap(slot.thumbnail);
  else if (slot.hasSwatch && slot.mediaResource.empty())
    activePreviewBitmap->SetBitmap(CreateSwatchBitmap(slot.swatch, wxSize(kActivePreviewSize, kActivePreviewSize)));
  else
    activePreviewBitmap->SetBitmap(CreatePlaceholderBitmap(wxSize(kActivePreviewSize, kActivePreviewSize)));

  if (!slot.previewStatus.empty())
    previewStatusCtrl->SetValue(wxString::FromUTF8(slot.previewStatus));
}

// Updates the preview pane when the user selects a wheel slot row.
void GdtfWheelInspectorPanel::OnSlotSelected(wxListEvent &event) {
  const long row = event.GetIndex();
  if (row < 0 || static_cast<size_t>(row) >= currentSlots.size())
    return;
  ApplySlotPreview(currentSlots[static_cast<size_t>(row)]);
}

// Creates a bitmap filled with the approximate display color for one slot.
wxBitmap GdtfWheelInspectorPanel::CreateSwatchBitmap(const wxColour &colour,
                                                     const wxSize &size) const {
  wxBitmap bitmap(std::max(1, size.GetWidth()), std::max(1, size.GetHeight()), 32);
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
