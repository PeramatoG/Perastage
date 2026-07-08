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

#include <functional>
#include <string>
#include <vector>

#include <wx/panel.h>
#include <wx/string.h>

class wxChoice;
class wxTextCtrl;

struct GdtfModeChannelPresentation {
  std::string channelLabel;
  std::string functionLabel;
};

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

  explicit GdtfModesPanel(wxWindow *parent);

  void SetPresentation(const GdtfModesPresentation &presentation);
  void SetModes(const std::vector<std::string> &modes);
  void SetSelectedMode(const std::string &mode);
  std::string GetSelectedMode() const;
  void SetModeSelectionCallback(ModeSelectionCallback callback);
  void SetChannelCount(const std::string &channelCount);
  void SetChannels(const std::vector<GdtfModeChannelPresentation> &channels);
  void ClearModeDetails();
  void SetModeSelectionEnabled(bool enabled);

private:
  void NotifyModeChanged();

  wxChoice *modeChoice = nullptr;
  wxTextCtrl *channelCountCtrl = nullptr;
  wxTextCtrl *channelListCtrl = nullptr;
  ModeSelectionCallback selectionCallback;
  bool updating = false;
};
