/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "gdtf_modes_panel.h"
#include "gdtf/gdtf_editor_visual_metrics.h"
#include "gdtf/gdtf_mode_data_view_model.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <optional>
#include <sstream>
#include <utility>

#include <wx/choice.h>
#include <wx/dataview.h>
#include <wx/sizer.h>
#include <wx/slider.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace {
// Returns a readable fallback for empty inspection text.
std::string InspectText(const std::string &value) { return value.empty() ? "-" : value; }

// Formats an integer with grouped thousands for compact inspection labels.
std::string FormatInspectionNumber(std::uint64_t value) {
  std::string text = std::to_string(value);
  for (int pos = static_cast<int>(text.size()) - 3; pos > 0; pos -= 3)
    text.insert(static_cast<size_t>(pos), ",");
  return text;
}

// Formats a DMX range for active state feedback.
std::string FormatInspectionRange(const std::optional<gdtf::GdtfDmxRange> &range) {
  if (!range)
    return "-";
  return FormatInspectionNumber(range->start) + "-" + FormatInspectionNumber(range->end);
}

// Formats a percentage with stable precision for inspection labels.
std::string FormatInspectionPercent(double percent) {
  std::ostringstream out;
  out << std::fixed << std::setprecision(2) << percent << "%";
  return out.str();
}

// Returns the wxSlider maximum for a DMX value domain.
int InspectionSliderMaxForDmxMax(std::uint64_t maxValue) {
  return static_cast<int>(std::min<std::uint64_t>(maxValue, 65535));
}

// Formats a compact value and percentage summary for the DMX inspection panel.
std::string FormatInspectionValueSummary(std::uint64_t value, std::uint64_t maxValue, double percent) {
  std::ostringstream out;
  out << "DMX value " << FormatInspectionNumber(value) << " of "
      << FormatInspectionNumber(maxValue) << " · " << FormatInspectionPercent(percent);
  return out.str();
}

// Formats the active mapping as compact, wrapped rows.
std::string FormatActiveMappingSummary(const gdtf::GdtfDmxInspectorMapping &mapping) {
  std::ostringstream out;
  out << "Function: " << InspectText(mapping.channelFunctionName)
      << " [" << FormatInspectionRange(mapping.channelFunctionDmxRange) << "]";
  if (!mapping.channelSetName.empty() || mapping.channelSetDmxRange)
    out << "\nState: " << InspectText(mapping.channelSetName)
        << " [" << FormatInspectionRange(mapping.channelSetDmxRange) << "]";
  if (mapping.wheel || mapping.slot) {
    out << "\nWheel: " << (mapping.wheel ? InspectText(mapping.wheel->name) : "-");
    if (mapping.slot)
      out << " / Slot " << mapping.slot->index << " " << InspectText(mapping.slot->name);
  }
  return out.str();
}

// Adds a non-empty detail row using a readable fallback.
void AddInspectorDetailRow(std::vector<GdtfWheelInspectorDetailRow> &rows,
                           std::string label, std::string value) {
  rows.push_back({std::move(label), InspectText(value)});
}

// Adds diagnostics to the detail table.
void AddInspectorDiagnosticRows(std::vector<GdtfWheelInspectorDetailRow> &rows,
                                const std::vector<gdtf::GdtfModeDiagnostic> &diagnostics) {
  for (const auto &diagnostic : diagnostics)
    AddInspectorDetailRow(rows, "Diagnostic", diagnostic.message);
}

// Builds the structured active-inspection table rows.
std::vector<GdtfWheelInspectorDetailRow> BuildActiveInspectorRows(
    const gdtf::GdtfDmxInspectionResult &result, double percent,
    const std::vector<GdtfWheelInspectorDetailRow> &selectedDetails) {
  std::vector<GdtfWheelInspectorDetailRow> rows;
  AddInspectorDetailRow(rows, "DMX value",
                        FormatInspectionNumber(result.normalizedValue) + " · " +
                            FormatInspectionPercent(percent));
  if (result.mappings.empty()) {
    AddInspectorDetailRow(rows, "Active", "No active mapping for this value.");
  } else {
    const auto &mapping = result.mappings.front();
    AddInspectorDetailRow(rows, "Attribute", mapping.logicalAttribute);
    AddInspectorDetailRow(rows, "Function", mapping.channelFunctionName);
    AddInspectorDetailRow(rows, "Function range", FormatInspectionRange(mapping.channelFunctionDmxRange));
    AddInspectorDetailRow(rows, "State", mapping.channelSetName);
    AddInspectorDetailRow(rows, "State range", FormatInspectionRange(mapping.channelSetDmxRange));
    AddInspectorDetailRow(rows, "Physical", mapping.physicalValue.empty()
                                        ? "-"
                                        : mapping.physicalValue + " " + InspectText(mapping.physicalUnit));
    if (mapping.wheel)
      AddInspectorDetailRow(rows, "Wheel", mapping.wheel->name);
    if (mapping.slot)
      AddInspectorDetailRow(rows, "Slot", std::to_string(mapping.slot->index) + " - " + mapping.slot->name);
    AddInspectorDetailRow(rows, "Media", mapping.mediaResource);
    AddInspectorDetailRow(rows, "Graphic resource", mapping.graphicWheelResource);
    AddInspectorDiagnosticRows(rows, mapping.diagnostics);
  }
  AddInspectorDiagnosticRows(rows, result.diagnostics);
  if (!selectedDetails.empty()) {
    AddInspectorDetailRow(rows, "Selected item", "Mode browser details");
    rows.insert(rows.end(), selectedDetails.begin(), selectedDetails.end());
  }
  return rows;
}

// Builds readable rows for a wheel slot preview.
std::vector<GdtfWheelInspectorDetailRow> BuildSlotInspectorRows(
    const gdtf::GdtfCatalogWheelSlotInfo &slot, const std::string &rawColor) {
  std::vector<GdtfWheelInspectorDetailRow> rows;
  AddInspectorDetailRow(rows, "Slot", std::to_string(slot.index) + " - " + slot.name);
  AddInspectorDetailRow(rows, "Media", slot.mediaFileName);
  AddInspectorDetailRow(rows, "Color", rawColor);
  AddInspectorDetailRow(rows, "Filter", slot.rawFilter);
  AddInspectorDetailRow(rows, "Graphic resource", slot.graphicWheelResource);
  return rows;
}

// Formats one active mapping for the side inspector.
void AppendMappingText(std::ostringstream &out, const gdtf::GdtfDmxInspectorMapping &mapping) {
  out << "Attribute: " << InspectText(mapping.logicalAttribute) << "\n";
  out << "Function: " << InspectText(mapping.channelFunctionName) << "\n";
  out << "Set: " << InspectText(mapping.channelSetName) << "\n";
  out << "Physical: " << InspectText(mapping.physicalValue) << " " << InspectText(mapping.physicalUnit) << "\n";
  out << "Wheel: " << (mapping.wheel ? InspectText(mapping.wheel->name) : "-") << "\n";
  out << "Slot: " << (mapping.slot ? std::to_string(mapping.slot->index) + " - " + InspectText(mapping.slot->name) : "-") << "\n";
  out << "Filter: " << (mapping.filter ? InspectText(mapping.filter->name) : "-") << "\n";
  out << "Media: " << InspectText(mapping.mediaResource) << "\n";
  out << "Graphic resource: " << InspectText(mapping.graphicWheelResource) << "\n";
  if (mapping.modeMasterConditional)
    out << "ModeMaster: conditional on " << InspectText(mapping.modeMaster) << "\n";
  if (mapping.physicalApproximate)
    out << "DMXProfile: physical value is approximate\n";
  for (const auto &diagnostic : mapping.diagnostics)
    out << "Diagnostic: " << diagnostic.message << "\n";
}

// Clamps the browser splitter ratio to a usable normalized range.
double ClampBrowserRatio(double ratio) { return std::clamp(ratio, 0.25, 0.85); }
}

// Formats a raw GDTF channel function label for display.
std::string FormatGdtfModeFunctionLabel(const std::string &functionText) {
  wxString function = wxString::FromUTF8(functionText);
  while (true) {
    const int sectionStart = function.Find('[');
    if (sectionStart == wxNOT_FOUND)
      break;
    const wxString remainder = function.Mid(static_cast<size_t>(sectionStart));
    const int sectionEndRelative = remainder.Find(']');
    if (sectionEndRelative == wxNOT_FOUND) {
      function = function.Left(static_cast<size_t>(sectionStart));
      break;
    }
    const size_t sectionEnd =
        static_cast<size_t>(sectionStart + sectionEndRelative);
    function = function.Left(static_cast<size_t>(sectionStart)) +
               function.Mid(sectionEnd + 1);
  }
  function.Trim(true).Trim(false);
  if (function.empty())
    function = "-";
  return std::string(function.ToUTF8());
}

// Creates the reusable modes and channels layout.
GdtfModesPanel::GdtfModesPanel(wxWindow *parent) : wxPanel(parent, wxID_ANY) {
  auto *root = new wxBoxSizer(wxVERTICAL);
  auto *grid = new wxFlexGridSizer(2, 5, 5);
  grid->AddGrowableCol(1, 1);
  modeChoice = new wxChoice(this, wxID_ANY);
  channelCountCtrl = new wxTextCtrl(this, wxID_ANY, wxString(),
                                    wxDefaultPosition, wxDefaultSize,
                                    wxTE_READONLY);
  grid->Add(new wxStaticText(this, wxID_ANY, _("Mode")), 0,
            wxALIGN_CENTER_VERTICAL);
  grid->Add(modeChoice, 1, wxEXPAND);
  grid->Add(new wxStaticText(this, wxID_ANY, _("Channel count")), 0,
            wxALIGN_CENTER_VERTICAL);
  grid->Add(channelCountCtrl, 1, wxEXPAND);
  root->Add(grid, 0, wxEXPAND | wxBOTTOM, 6);
  auto *inspectionBox = new wxStaticBoxSizer(wxVERTICAL, this, _("DMX inspection"));
  inspectionSlider = new wxSlider(this, wxID_ANY, 0, 0, 65535, wxDefaultPosition, wxDefaultSize);
  inspectionBox->Add(inspectionSlider, 0, wxEXPAND | wxBOTTOM, 4);
  auto *inspectionDetails = new wxFlexGridSizer(2, 4, 8);
  inspectionDetails->AddGrowableCol(1, 1);
  inspectionValueLabel = new wxStaticText(this, wxID_ANY, _("DMX value 0 of 65,535 · 0.00%"));
  inspectionDetails->Add(new wxStaticText(this, wxID_ANY, _("Value")), 0, wxALIGN_TOP | wxTOP, 2);
  inspectionDetails->Add(inspectionValueLabel, 1, wxEXPAND);
  inspectionMappingLabel = new wxTextCtrl(this, wxID_ANY, "Select a DMX channel to inspect its active function and wheel slot.",
                                          wxDefaultPosition, wxSize(-1, gui::gdtf_layout::Dip(this, 72)),
                                          wxTE_MULTILINE | wxTE_READONLY | wxBORDER_NONE | wxTE_WORDWRAP);
  inspectionMappingLabel->SetMinSize(wxSize(-1, gui::gdtf_layout::Dip(this, 72)));
  inspectionMappingLabel->SetBackgroundColour(GetBackgroundColour());
  inspectionMappingLabel->SetForegroundColour(GetForegroundColour());
  inspectionDetails->Add(new wxStaticText(this, wxID_ANY, _("Active")), 0, wxALIGN_TOP | wxTOP, 2);
  inspectionDetails->Add(inspectionMappingLabel, 1, wxEXPAND);
  inspectionBox->Add(inspectionDetails, 0, wxEXPAND);
  root->Add(inspectionBox, 0, wxEXPAND | wxBOTTOM, 6);
  root->Add(new wxStaticText(this, wxID_ANY, _("Mode and channel browser")), 0,
            wxBOTTOM, 3);

  browserCtrl = new wxDataViewCtrl(this, wxID_ANY, wxDefaultPosition,
                                   wxSize(-1, gui::gdtf_layout::Dip(this, 300)),
                                   wxDV_ROW_LINES | wxDV_VERT_RULES);
  browserCtrl->AppendTextColumn(_("Item"), GdtfModeDataViewModel::Item, wxDATAVIEW_CELL_INERT, gui::gdtf_layout::Dip(this, 180), wxALIGN_LEFT, 0);
  browserCtrl->AppendTextColumn(_("DMX range"), GdtfModeDataViewModel::DmxRange, wxDATAVIEW_CELL_INERT, gui::gdtf_layout::Dip(this, 90), wxALIGN_LEFT, 0);
  browserCtrl->AppendTextColumn(_("Physical range"), GdtfModeDataViewModel::PhysicalRange, wxDATAVIEW_CELL_INERT, gui::gdtf_layout::Dip(this, 130), wxALIGN_LEFT, 0);
  browserCtrl->AppendTextColumn(_("Unit"), GdtfModeDataViewModel::Unit, wxDATAVIEW_CELL_INERT, gui::gdtf_layout::Dip(this, 80), wxALIGN_LEFT, 0);
  browserModel = new GdtfModeDataViewModel();
  browserCtrl->AssociateModel(browserModel);
  browserModel->DecRef();
  browserCtrl->SetMinSize(wxSize(-1, gui::gdtf_layout::Dip(this, 180)));
  root->Add(browserCtrl, 1, wxEXPAND);

  modeChoice->Bind(wxEVT_CHOICE, [this](wxCommandEvent &) { NotifyModeChanged(); });
  inspectionSlider->Bind(wxEVT_SLIDER, [this](wxCommandEvent &) { UpdateInspectionFromSlider(); });
  browserCtrl->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED, [this](wxDataViewEvent &event) {
    const auto *node = browserModel ? browserModel->GetNode(event.GetItem()) : nullptr;
    if (node) {
      selectedByMode[GetSelectedMode()] = node->id;
      SelectInspectionNode(node->id);
    }
    UpdateDetails(node);
  });
  SetSizer(root);
}

// Applies a complete modes presentation without notifying the host.
void GdtfModesPanel::SetPresentation(const GdtfModesPresentation &presentation) {
  SetModes(presentation.modes);
  SetSelectedMode(presentation.selectedMode);
  SetChannelCount(presentation.channelCount);
  SetChannels(presentation.channels);
}

// Replaces the ordered mode list without notifying the host.
void GdtfModesPanel::SetModes(const std::vector<std::string> &modes) {
  updating = true;
  modeChoice->Clear();
  for (const auto &mode : modes)
    modeChoice->Append(wxString::FromUTF8(mode));
  updating = false;
}

// Selects a mode without notifying the host.
void GdtfModesPanel::SetSelectedMode(const std::string &mode) {
  updating = true;
  const int selection = modeChoice->FindString(wxString::FromUTF8(mode));
  if (selection != wxNOT_FOUND)
    modeChoice->SetSelection(selection);
  else
    modeChoice->SetSelection(wxNOT_FOUND);
  updating = false;
}

// Returns the selected mode exactly as shown by the choice control.
std::string GdtfModesPanel::GetSelectedMode() const {
  return std::string(modeChoice->GetStringSelection().ToUTF8());
}

// Registers the host mode-selection callback.
void GdtfModesPanel::SetModeSelectionCallback(ModeSelectionCallback callback) {
  selectionCallback = std::move(callback);
}

// Registers the host wheel-inspection presentation callback.
void GdtfModesPanel::SetWheelInspectionCallback(WheelInspectionCallback callback) {
  wheelInspectionCallback = std::move(callback);
}

// Sets the derived channel count text.
void GdtfModesPanel::SetChannelCount(const std::string &channelCount) {
  updating = true;
  channelCountCtrl->SetValue(wxString::FromUTF8(channelCount));
  updating = false;
}

// Ignores legacy summary rows because they are displayed by GdtfChannelSummaryPanel.
void GdtfModesPanel::SetChannels(const std::vector<GdtfModeChannelPresentation> &) {}

// Sets the ordered read-only hierarchical browser nodes.
void GdtfModesPanel::SetBrowserNodes(const std::vector<GdtfModeBrowserNodePresentation> &nodes) {
  RememberExpandedItems();
  updating = true;
  browserModel->SetNodes(nodes);
  RestoreExpandedItems();
  wxDataViewItem selected;
  auto selectedIt = selectedByMode.find(GetSelectedMode());
  if (selectedIt != selectedByMode.end())
    selected = browserModel->GetItemById(selectedIt->second);
  if (!selected.IsOk()) {
    auto roots = browserModel->GetTopLevelItems();
    if (!roots.empty())
      selected = roots.front();
  }
  if (selected.IsOk())
    browserCtrl->Select(selected);
  if (const auto *node = browserModel->GetNode(selected))
    SelectInspectionNode(node->id);
  UpdateDetails(browserModel->GetNode(selected));
  updating = false;
}

// Applies the typed inspection model used by the read-only slider resolver.
void GdtfModesPanel::SetInspectionData(const gdtf::GdtfDmxModeNode *mode,
                                       const gdtf::GdtfWheelCatalog *catalog) {
  hasInspectionData = mode && catalog;
  inspectionMode = mode ? *mode : gdtf::GdtfDmxModeNode{};
  inspectionCatalog = catalog ? *catalog : gdtf::GdtfWheelCatalog{};
  selectedInspectionChannelId.clear();
  inspectionValueByChannel.clear();
  UpdateInspectionSliderRange();
  SetInspectionMappingText("Select a DMX channel to inspect its active function and wheel slot.");
  if (wheelInspectionCallback)
    wheelInspectionCallback({"Select a DMX channel and move the inspection slider to resolve wheel and slot data.", {}});
}

// Sets the normalized browser/details splitter ratio.
void GdtfModesPanel::SetBrowserSplitterRatio(double ratio) {
  browserSplitterRatio = ClampBrowserRatio(ratio);
}

// Returns the stored browser/details splitter ratio for layout preference compatibility.
double GdtfModesPanel::GetBrowserSplitterRatio() const {
  return browserSplitterRatio;
}

// Sets the read-only DMX inspection value label.
void GdtfModesPanel::SetInspectionValueText(const std::string &text) {
  if (inspectionValueLabel)
    inspectionValueLabel->SetLabel(wxString::FromUTF8(text));
}

// Sets the wrapped DMX inspection active-mapping summary.
void GdtfModesPanel::SetInspectionMappingText(const std::string &text) {
  if (inspectionMappingLabel)
    inspectionMappingLabel->ChangeValue(wxString::FromUTF8(text));
}

// Clears the derived channel presentation.
void GdtfModesPanel::ClearModeDetails() {
  updating = true;
  channelCountCtrl->SetValue(wxString());
  browserModel->SetNodes({});
  selectedInspectionChannelId.clear();
  inspectionValueByChannel.clear();
  if (inspectionSlider) {
    UpdateInspectionSliderRange();
    inspectionSlider->SetValue(0);
  }
  if (inspectionValueLabel)
    inspectionValueLabel->SetLabel(_("DMX value 0 of 65,535 · 0.00%"));
  SetInspectionMappingText("Select a DMX channel to inspect its active function and wheel slot.");
  if (wheelInspectionCallback)
    wheelInspectionCallback({"Select a DMX channel and move the inspection slider to resolve wheel and slot data.", {}});
  updating = false;
}

// Enables or disables mode selection.
void GdtfModesPanel::SetModeSelectionEnabled(bool enabled) { modeChoice->Enable(enabled); }

// Notifies the host about a genuine user mode selection.
void GdtfModesPanel::NotifyModeChanged() {
  if (updating || !selectionCallback)
    return;
  RememberExpandedItems();
  selectionCallback(GetSelectedMode());
}

// Updates the read-only key/value details inspector for the selected node.
void GdtfModesPanel::UpdateDetails(const GdtfModeBrowserNodePresentation *node) {
  selectedInspectionDetails.clear();
  if (node) {
    for (const auto &row : node->details)
      selectedInspectionDetails.push_back({row.key, row.value});
  }
  if (!updating && hasInspectionData && !selectedInspectionChannelId.empty())
    UpdateInspectionFromSlider();
}

// Selects the owning DMX channel for the selected browser node.
void GdtfModesPanel::SelectInspectionNode(const std::string &nodeId) {
  const auto *channel = FindOwningChannel(nodeId);
  if (!channel) {
    selectedInspectionChannelId.clear();
    UpdateInspectionSliderRange();
    SetInspectionMappingText("Select a DMX channel to inspect its active function and wheel slot.");
    return;
  }
  selectedInspectionChannelId = channel->id;
  UpdateInspectionSliderRange();
  const auto cachedValue = inspectionValueByChannel.find(selectedInspectionChannelId);
  const std::uint64_t restoredValue = cachedValue == inspectionValueByChannel.end()
                                          ? 0
                                          : cachedValue->second;
  if (inspectionSlider)
    inspectionSlider->SetValue(SliderValueFromDmxValue(restoredValue));
  UpdateInspectionFromSlider();
}

// Updates the read-only inspection labels and wheel panel from the slider value.
void GdtfModesPanel::UpdateInspectionFromSlider() {
  const std::uint64_t value = CurrentInspectionValue();
  const std::uint64_t maxValue = SelectedInspectionMaxValue();
  const double percent = maxValue > 0
                             ? static_cast<double>(value) * 100.0 / static_cast<double>(maxValue)
                             : 0.0;
  if (!selectedInspectionChannelId.empty())
    inspectionValueByChannel[selectedInspectionChannelId] = value;
  if (inspectionValueLabel) {
    inspectionValueLabel->SetLabel(wxString::FromUTF8(FormatInspectionValueSummary(value, maxValue, percent)));
  }
  if (!hasInspectionData || selectedInspectionChannelId.empty()) {
    SetInspectionMappingText("Select a DMX channel to inspect its active function and wheel slot.");
    return;
  }
  const auto result = gdtf::InspectGdtfDmxValue(inspectionMode, selectedInspectionChannelId,
                                               value, inspectionCatalog);
  std::ostringstream active;
  active << "DMX value: " << result.normalizedValue << " / "
         << FormatInspectionPercent(percent) << "\n";
  for (size_t i = 0; i < result.mappings.size(); ++i) {
    if (i > 0)
      active << "\n";
    AppendMappingText(active, result.mappings[i]);
  }
  for (const auto &diagnostic : result.diagnostics)
    active << "Diagnostic: " << diagnostic.message << "\n";

  std::string label = "No active mapping for this value.";
  if (!result.mappings.empty())
    label = FormatActiveMappingSummary(result.mappings.front());
  SetInspectionMappingText(label);

  GdtfWheelInspectorPresentation presentation;
  presentation.activeText = active.str();
  presentation.detailRows = BuildActiveInspectorRows(result, percent, selectedInspectionDetails);
  const gdtf::GdtfCatalogWheelInfo *galleryWheel = nullptr;
  const gdtf::GdtfCatalogWheelSlotInfo *selectedSlot = nullptr;
  for (const auto &mapping : result.mappings) {
    if (mapping.wheel) {
      galleryWheel = mapping.wheel;
      selectedSlot = mapping.slot;
      break;
    }
  }
  if (galleryWheel) {
    for (const auto &slot : galleryWheel->slots) {
      std::string slotText = std::to_string(slot.index) + ". " + InspectText(slot.name);
      if (!slot.mediaFileName.empty())
        slotText += " | media " + slot.mediaFileName;
      if (!slot.rawColor.empty())
        slotText += " | color " + slot.rawColor;
      if (!slot.rawFilter.empty())
        slotText += " | filter " + slot.rawFilter;
      if (!slot.graphicWheelResource.empty())
        slotText += " | graphic " + slot.graphicWheelResource;
      std::string rawColor = slot.rawColor;
      if (rawColor.empty() && !slot.rawFilter.empty()) {
        if (const auto *slotFilter = inspectionCatalog.FindFilter(slot.rawFilter))
          rawColor = slotFilter->rawColor;
      }
      GdtfWheelInspectorSlotPresentation slotPresentation;
      slotPresentation.label = slotText;
      slotPresentation.detailRows = BuildSlotInspectorRows(slot, rawColor);
      slotPresentation.mediaResource = slot.mediaFileName;
      slotPresentation.graphicResource = slot.graphicWheelResource;
      slotPresentation.rawColor = rawColor;
      slotPresentation.selected = selectedSlot && selectedSlot->id == slot.id;
      if (slotPresentation.selected)
        presentation.previewRows = slotPresentation.detailRows;
      presentation.slots.push_back(std::move(slotPresentation));
    }
  }
  if (wheelInspectionCallback)
    wheelInspectionCallback(presentation);
}

// Finds the DMX channel that owns a browser node id.
const gdtf::GdtfDmxChannelNode *GdtfModesPanel::FindOwningChannel(
    const std::string &nodeId) const {
  if (!hasInspectionData)
    return nullptr;
  for (const auto &channel : inspectionMode.channels) {
    if (nodeId == channel.id || nodeId.rfind(channel.id + "/", 0) == 0)
      return &channel;
  }
  return nullptr;
}

// Updates the slider range to match the selected DMX channel resolution.
void GdtfModesPanel::UpdateInspectionSliderRange() {
  if (!inspectionSlider)
    return;
  inspectionSlider->SetRange(0, InspectionSliderMaxForDmxMax(SelectedInspectionMaxValue()));
}

// Returns the current slider value as an inspection DMX value.
std::uint64_t GdtfModesPanel::CurrentInspectionValue() const {
  if (!inspectionSlider)
    return 0;
  const std::uint64_t maxValue = SelectedInspectionMaxValue();
  const std::uint64_t sliderValue = static_cast<std::uint64_t>(inspectionSlider->GetValue());
  if (maxValue <= 65535)
    return std::min(sliderValue, maxValue);
  const long double scaled = static_cast<long double>(sliderValue) *
                             static_cast<long double>(maxValue) / 65535.0L;
  return static_cast<std::uint64_t>(std::llround(scaled));
}

// Returns the selected channel maximum value for resolution-aware inspection.
std::uint64_t GdtfModesPanel::SelectedInspectionMaxValue() const {
  const auto *channel = FindOwningChannel(selectedInspectionChannelId);
  if (!channel)
    return 65535;
  const int resolution = std::max(1, channel->resolution);
  if (resolution >= 8)
    return UINT64_MAX;
  return (std::uint64_t{1} << (8 * resolution)) - 1;
}

// Converts an exact DMX value to the fixed slider scale.
int GdtfModesPanel::SliderValueFromDmxValue(std::uint64_t value) const {
  const std::uint64_t maxValue = SelectedInspectionMaxValue();
  if (maxValue <= 65535)
    return static_cast<int>(std::min(value, maxValue));
  const long double scaled = static_cast<long double>(std::min(value, maxValue)) *
                             65535.0L / static_cast<long double>(maxValue);
  return static_cast<int>(std::llround(scaled));
}

// Remembers expanded browser item identities for the active mode.
void GdtfModesPanel::RememberExpandedItems() {
  if (!browserModel || !browserCtrl)
    return;
  auto &expanded = expandedByMode[GetSelectedMode()];
  expanded.clear();
  wxDataViewItemArray children;
  browserModel->GetChildren(wxDataViewItem(), children);
  for (auto item : children) {
    if (browserCtrl->IsExpanded(item)) {
      if (const auto *node = browserModel->GetNode(item))
        expanded.insert(node->id);
    }
  }
}

// Restores mode-specific expansion or expands DMX Channel roots initially.
void GdtfModesPanel::RestoreExpandedItems() {
  if (!browserModel || !browserCtrl)
    return;
  const auto mode = GetSelectedMode();
  const auto it = expandedByMode.find(mode);
  for (auto item : browserModel->GetTopLevelItems()) {
    const auto *node = browserModel->GetNode(item);
    if (it == expandedByMode.end() || (node && it->second.count(node->id)))
      browserCtrl->Expand(item);
  }
}
