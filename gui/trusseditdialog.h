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

#include <array>
#include <string>
#include <vector>

#include <wx/wx.h>

class FixturePreviewPanel;
class GdtfMetadataPanel;
class TrussTablePanel;

class TrussEditDialog : public wxDialog {
public:
  TrussEditDialog(TrussTablePanel *panel, int row);
  bool WasApplied() const { return applied; }

private:
  void MarkColumnModified(size_t index);
  void OnApply(wxCommandEvent &event);
  void OnOk(wxCommandEvent &event);
  void OnCancel(wxCommandEvent &event);
  void ApplyChanges();
  bool EnsureGdtfForEditedTruss();
  std::string ResolveCurrentGdtfPath() const;
  void UpdateMetadataSummary();
  void UpdatePreview();

  TrussTablePanel *panel = nullptr;
  int row = -1;
  std::vector<wxControl *> ctrls;
  wxTextCtrl *crossSectionCtrl = nullptr;
  GdtfMetadataPanel *metadataPanel = nullptr;
  FixturePreviewPanel *preview = nullptr;
  std::vector<bool> modifiedColumns;
  bool crossSectionModified = false;
  bool applied = false;
};
