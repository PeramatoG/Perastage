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
#include <memory>
#include <map>
#include <string>
#include <vector>

#include "gdtf/editor/gdtf_field_registry.h"
#include <wx/wx.h>

class FixturePreviewPanel;
class GdtfEditorPanel;
class TrussTablePanel;
namespace gdtf { class GdtfEditSession; }

class TrussEditDialog : public wxDialog {
public:
  TrussEditDialog(TrussTablePanel *panel, int row);
  ~TrussEditDialog() override;
  bool WasApplied() const { return applied; }

private:
  void MarkColumnModified(size_t index);
  void OnApply(wxCommandEvent &event);
  void OnOk(wxCommandEvent &event);
  void OnCancel(wxCommandEvent &event);
  bool ApplyChanges();
  bool EnsureGdtfForEditedTruss();
  std::string ResolveCurrentGdtfPath() const;
  void UpdateMetadataSummary();
  void UpdatePreview();
  void BuildEditSession();
  void SyncSessionDirtyToLegacyFlags();
  bool SetSessionValue(gdtf::GdtfFieldId fieldId, const std::string &value);
  bool ValidateSessionBeforeApply();
  void ClearSessionValidation();

  TrussTablePanel *panel = nullptr;
  int row = -1;
  std::vector<wxControl *> ctrls;
  GdtfEditorPanel *gdtfEditorPanel = nullptr;
  std::unique_ptr<gdtf::GdtfEditSession> gdtfEditSession;
  FixturePreviewPanel *preview = nullptr;
  std::vector<bool> modifiedColumns;
  bool crossSectionModified = false;
  std::map<gdtf::GdtfFieldId, std::string> rejectedSessionInputs;
  bool applied = false;
};
