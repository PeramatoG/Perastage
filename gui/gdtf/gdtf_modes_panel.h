/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include "gdtf/gdtf_mode_browser_presenter.h"
#include "gdtf/gdtf_dmx_inspector.h"
#include "gdtf/gdtf_wheel_catalog.h"
#include "gdtf_wheel_inspector_panel.h"

#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <wx/panel.h>
#include <wx/string.h>

class wxChoice;
class wxDataViewCtrl;
class wxSlider;
class wxSplitterWindow;
class wxStaticText;
class wxTextCtrl;
class GdtfModeDataViewModel;

struct GdtfModesPresentation {
  std::vector<std::string> modes;
  std::string selectedMode;
  std::string channelCount;
  std::vector<GdtfModeChannelPresentation> channels;
};

std::string FormatGdtfModeFunctionLabel(const std::string &functionText);

class GdtfModesPanel : public wxPanel {
public:
  using ModeSelectionCallback = std::function<void(const std::string &mode)>;
  using WheelInspectionCallback = std::function<void(const GdtfWheelInspectorPresentation &presentation)>;

  explicit GdtfModesPanel(wxWindow *parent);

  void SetPresentation(const GdtfModesPresentation &presentation);
  void SetModes(const std::vector<std::string> &modes);
  void SetSelectedMode(const std::string &mode);
  std::string GetSelectedMode() const;
  void SetModeSelectionCallback(ModeSelectionCallback callback);
  void SetWheelInspectionCallback(WheelInspectionCallback callback);
  void SetChannelCount(const std::string &channelCount);
  void SetChannels(const std::vector<GdtfModeChannelPresentation> &channels);
  void SetBrowserNodes(const std::vector<GdtfModeBrowserNodePresentation> &nodes);
  void SetInspectionData(const gdtf::GdtfDmxModeNode *mode, const gdtf::GdtfWheelCatalog *catalog);
  void SetBrowserSplitterRatio(double ratio);
  double GetBrowserSplitterRatio() const;
  void ClearModeDetails();
  void SetInspectionValueText(const std::string &text);
  void SetModeSelectionEnabled(bool enabled);

private:
  void NotifyModeChanged();
  void UpdateDetails(const GdtfModeBrowserNodePresentation *node);
  void SelectInspectionNode(const std::string &nodeId);
  void UpdateInspectionFromSlider();
  void UpdateInspectionSliderRange();
  void SetInspectionMappingText(const std::string &text);
  const gdtf::GdtfDmxChannelNode *FindOwningChannel(const std::string &nodeId) const;
  std::uint64_t CurrentInspectionValue() const;
  std::uint64_t SelectedInspectionMaxValue() const;
  int SliderValueFromDmxValue(std::uint64_t value) const;
  void RememberExpandedItems();
  void RestoreExpandedItems();

  wxChoice *modeChoice = nullptr;
  wxTextCtrl *channelCountCtrl = nullptr;
  wxStaticText *inspectionValueLabel = nullptr;
  wxTextCtrl *inspectionMappingLabel = nullptr;
  wxSlider *inspectionSlider = nullptr;
  wxSplitterWindow *browserSplitter = nullptr;
  wxDataViewCtrl *browserCtrl = nullptr;
  wxTextCtrl *detailsCtrl = nullptr;
  GdtfModeDataViewModel *browserModel = nullptr;
  std::map<std::string, std::set<std::string>> expandedByMode;
  std::map<std::string, std::string> selectedByMode;
  double browserSplitterRatio = 0.68;
  ModeSelectionCallback selectionCallback;
  WheelInspectionCallback wheelInspectionCallback;
  gdtf::GdtfDmxModeNode inspectionMode;
  gdtf::GdtfWheelCatalog inspectionCatalog;
  std::string selectedInspectionChannelId;
  std::vector<GdtfWheelInspectorDetailRow> selectedInspectionDetails;
  bool hasInspectionData = false;
  bool updating = false;
};
