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
#include <sstream>
#include <utility>

#include <wx/choice.h>
#include <wx/dataview.h>
#include <wx/sizer.h>
#include <wx/slider.h>
#include <wx/splitter.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace {
// Returns a readable fallback for empty inspection text.
std::string InspectText(const std::string &value) { return value.empty() ? "-" : value; }

// Joins numeric DMX bytes for compact inspection feedback.
std::string FormatInspectionBytes(const std::vector<unsigned int> &bytes) {
  std::string result;
  for (const auto value : bytes) {
    if (!result.empty())
      result += ",";
    result += std::to_string(value);
  }
  return result.empty() ? "-" : result;
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
  grid->Add(new wxStaticText(this, wxID_ANY, "Mode"), 0,
            wxALIGN_CENTER_VERTICAL);
  grid->Add(modeChoice, 1, wxEXPAND);
  grid->Add(new wxStaticText(this, wxID_ANY, "Channel count"), 0,
            wxALIGN_CENTER_VERTICAL);
  grid->Add(channelCountCtrl, 1, wxEXPAND);
  root->Add(grid, 0, wxEXPAND | wxBOTTOM, 6);
  auto *inspectionRow = new wxBoxSizer(wxHORIZONTAL);
  inspectionRow->Add(new wxStaticText(this, wxID_ANY, "DMX inspection"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
  inspectionSlider = new wxSlider(this, wxID_ANY, 0, 0, 65535, wxDefaultPosition, wxDefaultSize);
  inspectionValueLabel = new wxStaticText(this, wxID_ANY, "Value 0 / 0x00 / 0.00% / bytes 0");
  inspectionRow->Add(inspectionSlider, 1, wxEXPAND | wxRIGHT, 6);
  inspectionRow->Add(inspectionValueLabel, 0, wxALIGN_CENTER_VERTICAL);
  root->Add(inspectionRow, 0, wxEXPAND | wxBOTTOM, 3);
  inspectionMappingLabel = new wxStaticText(this, wxID_ANY, "Select a DMX channel to inspect its active function and wheel slot.");
  root->Add(inspectionMappingLabel, 0, wxEXPAND | wxBOTTOM, 6);
  root->Add(new wxStaticText(this, wxID_ANY, "Mode and channel browser"), 0,
            wxBOTTOM, 3);

  browserSplitter = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition,
                                         wxSize(-1, gui::gdtf_layout::Dip(this, 300)),
                                         wxSP_LIVE_UPDATE | wxSP_3DSASH);
  browserCtrl = new wxDataViewCtrl(browserSplitter, wxID_ANY, wxDefaultPosition,
                                   wxDefaultSize, wxDV_ROW_LINES | wxDV_VERT_RULES);
  browserCtrl->AppendTextColumn("Item", GdtfModeDataViewModel::Item, wxDATAVIEW_CELL_INERT, gui::gdtf_layout::Dip(this, 180), wxALIGN_LEFT, 0);
  browserCtrl->AppendTextColumn("DMX range", GdtfModeDataViewModel::DmxRange, wxDATAVIEW_CELL_INERT, gui::gdtf_layout::Dip(this, 90), wxALIGN_LEFT, 0);
  browserCtrl->AppendTextColumn("Physical range", GdtfModeDataViewModel::PhysicalRange, wxDATAVIEW_CELL_INERT, gui::gdtf_layout::Dip(this, 130), wxALIGN_LEFT, 0);
  browserCtrl->AppendTextColumn("Unit", GdtfModeDataViewModel::Unit, wxDATAVIEW_CELL_INERT, gui::gdtf_layout::Dip(this, 80), wxALIGN_LEFT, 0);
  browserModel = new GdtfModeDataViewModel();
  browserCtrl->AssociateModel(browserModel);
  browserModel->DecRef();
  detailsCtrl = new wxTextCtrl(browserSplitter, wxID_ANY, wxString(), wxDefaultPosition,
                               wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);
  browserCtrl->SetMinSize(wxSize(-1, gui::gdtf_layout::Dip(this, 180)));
  detailsCtrl->SetMinSize(wxSize(-1, gui::gdtf_layout::Dip(this, 90)));
  browserSplitter->SplitHorizontally(browserCtrl, detailsCtrl,
      gui::gdtf_layout::Dip(this, 204));
  root->Add(browserSplitter, 1, wxEXPAND);

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
  if (inspectionMappingLabel)
    inspectionMappingLabel->SetLabel("Select a DMX channel to inspect its active function and wheel slot.");
  if (wheelInspectionCallback)
    wheelInspectionCallback({"Select a DMX channel and move the inspection slider to resolve wheel and slot data.", {}});
}

// Sets the normalized browser/details splitter ratio.
void GdtfModesPanel::SetBrowserSplitterRatio(double ratio) {
  browserSplitterRatio = ClampBrowserRatio(ratio);
  if (!browserSplitter)
    return;
  const int height = browserSplitter->GetClientSize().GetHeight();
  if (height > 0)
    browserSplitter->SetSashPosition(static_cast<int>(height * browserSplitterRatio));
}

// Returns the current normalized browser/details splitter ratio.
double GdtfModesPanel::GetBrowserSplitterRatio() const {
  if (!browserSplitter)
    return browserSplitterRatio;
  const int height = browserSplitter->GetClientSize().GetHeight();
  if (height <= 0)
    return browserSplitterRatio;
  return ClampBrowserRatio(static_cast<double>(browserSplitter->GetSashPosition()) / height);
}

// Sets the read-only DMX inspection value label.
void GdtfModesPanel::SetInspectionValueText(const std::string &text) {
  if (inspectionValueLabel)
    inspectionValueLabel->SetLabel(wxString::FromUTF8(text));
}

// Clears the derived channel presentation.
void GdtfModesPanel::ClearModeDetails() {
  updating = true;
  channelCountCtrl->SetValue(wxString());
  browserModel->SetNodes({});
  detailsCtrl->SetValue(wxString());
  if (inspectionSlider)
    inspectionSlider->SetValue(0);
  if (inspectionValueLabel)
    inspectionValueLabel->SetLabel("Value 0 / 0x00 / 0.00% / bytes 0");
  if (inspectionMappingLabel)
    inspectionMappingLabel->SetLabel("Select a DMX channel to inspect its active function and wheel slot.");
  selectedInspectionChannelId.clear();
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
  wxString text;
  if (node) {
    for (const auto &row : node->details)
      text += wxString::FromUTF8(row.key) + ": " + wxString::FromUTF8(row.value) + "\n";
  }
  detailsCtrl->SetValue(text);
}

// Selects the owning DMX channel for the selected browser node.
void GdtfModesPanel::SelectInspectionNode(const std::string &nodeId) {
  const auto *channel = FindOwningChannel(nodeId);
  if (!channel) {
    selectedInspectionChannelId.clear();
    if (inspectionMappingLabel)
      inspectionMappingLabel->SetLabel("Select a DMX channel to inspect its active function and wheel slot.");
    return;
  }
  selectedInspectionChannelId = channel->id;
  std::uint64_t startValue = 0;
  for (const auto &logical : channel->logicalChannels) {
    for (const auto &function : logical.channelFunctions) {
      if (function.id == nodeId && function.effectiveDmxRange) {
        startValue = function.effectiveDmxRange->start;
        break;
      }
      for (const auto &set : function.channelSets) {
        if (set.id == nodeId && set.effectiveDmxRange) {
          startValue = set.effectiveDmxRange->start;
          break;
        }
      }
    }
  }
  if (inspectionSlider)
    inspectionSlider->SetValue(static_cast<int>(std::min<std::uint64_t>(startValue, 65535)));
  UpdateInspectionFromSlider();
}

// Updates the read-only inspection labels and wheel panel from the slider value.
void GdtfModesPanel::UpdateInspectionFromSlider() {
  const std::uint64_t value = CurrentInspectionValue();
  const double percent = static_cast<double>(value) * 100.0 / 65535.0;
  if (inspectionValueLabel) {
    std::ostringstream valueText;
    valueText << "Value " << value << " / 0x" << std::uppercase << std::hex << value
              << std::dec << " / " << percent << "% / bytes "
              << static_cast<unsigned>((value >> 8) & 0xff) << ","
              << static_cast<unsigned>(value & 0xff);
    inspectionValueLabel->SetLabel(wxString::FromUTF8(valueText.str()));
  }
  if (!hasInspectionData || selectedInspectionChannelId.empty()) {
    if (inspectionMappingLabel)
      inspectionMappingLabel->SetLabel("Select a DMX channel to inspect its active function and wheel slot.");
    return;
  }
  const auto result = gdtf::InspectGdtfDmxValue(inspectionMode, selectedInspectionChannelId,
                                               value, inspectionCatalog);
  std::ostringstream active;
  active << "DMX value: " << result.normalizedValue << " / bytes "
         << FormatInspectionBytes(result.bytes) << "\n";
  for (size_t i = 0; i < result.mappings.size(); ++i) {
    if (i > 0)
      active << "\n";
    AppendMappingText(active, result.mappings[i]);
  }
  for (const auto &diagnostic : result.diagnostics)
    active << "Diagnostic: " << diagnostic.message << "\n";

  std::string label = "No active mapping for this value.";
  if (!result.mappings.empty()) {
    const auto &mapping = result.mappings.front();
    label = "Active: " + InspectText(mapping.channelFunctionName) + " / " +
            InspectText(mapping.channelSetName);
    if (mapping.wheel)
      label += " / Wheel " + InspectText(mapping.wheel->name);
    if (mapping.slot)
      label += " / Slot " + std::to_string(mapping.slot->index) + " " + InspectText(mapping.slot->name);
  }
  if (inspectionMappingLabel)
    inspectionMappingLabel->SetLabel(wxString::FromUTF8(label));

  GdtfWheelInspectorPresentation presentation;
  presentation.activeText = active.str();
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
      slotPresentation.mediaResource = slot.mediaFileName;
      slotPresentation.graphicResource = slot.graphicWheelResource;
      slotPresentation.rawColor = rawColor;
      slotPresentation.selected = selectedSlot && selectedSlot->id == slot.id;
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

// Returns the current slider value as an inspection DMX value.
std::uint64_t GdtfModesPanel::CurrentInspectionValue() const {
  return inspectionSlider ? static_cast<std::uint64_t>(inspectionSlider->GetValue()) : 0;
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
