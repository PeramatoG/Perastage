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

#include <wx/dataview.h>
#include <wx/notebook.h>
#include <wx/wx.h>

#include "dictionary_editor_state.h"
#include "dictionary_selection_controls.h"

#include <string>
#include <vector>

class ColorfulDataViewListStore;

class DictionaryEditDialog : public wxDialog {
public:
  explicit DictionaryEditDialog(wxWindow *parent);

private:
  void BuildLayout();
  void LoadFixtures();
  void LoadTrusses();
  bool SaveFixtures();
  bool SaveTrusses();
  std::vector<std::string> BuildFixtureSnapshotFromUi() const;
  std::vector<std::string> BuildTrussSnapshotFromUi() const;
  bool HasFixtureChanges() const;
  bool HasTrussChanges() const;
  bool ConfirmDirtyChangesBeforeReload(const wxString &operationLabel);
  void ShowDictionaryLoadStatusMessages();
  bool IsFixturesPage() const;
  void SyncFixtureVisualColorForFileAndMode(int row);
  void UpdateFixtureCategoryForFile(int row, const std::string &category);
  void UpdateFixtureVisualColorForFileAndMode(int row,
                                              const std::string &colorHex);
  void OnFixtureTableMouseMove(wxMouseEvent &event);
  void OnFixtureTableMouseLeave(wxMouseEvent &event);
  void OnTrussTableMouseMove(wxMouseEvent &event);
  void OnTrussTableMouseLeave(wxMouseEvent &event);
  void UpdateMissingFileTooltip(wxDataViewListCtrl *table,
                                ColorfulDataViewListStore *store,
                                wxString &activeTooltip,
                                const wxPoint &position);
  void RefreshDictionarySelectionLabels();
  void OnOpenFixturesDictionary(wxCommandEvent &event);
  void OnOpenTrussesDictionary(wxCommandEvent &event);
  void OnNewFixturesDictionary(wxCommandEvent &event);
  void OnNewTrussesDictionary(wxCommandEvent &event);
  void OnDuplicateFixturesDictionary(wxCommandEvent &event);
  void OnDuplicateTrussesDictionary(wxCommandEvent &event);
  void OnUseDefaultFixturesDictionary(wxCommandEvent &event);
  void OnUseDefaultTrussesDictionary(wxCommandEvent &event);

  void OnAdd(wxCommandEvent &event);
  void OnDelete(wxCommandEvent &event);
  void OnDownloadGdtf(wxCommandEvent &event);
  void OnImportDictionary(wxCommandEvent &event);
  void OnExportDictionary(wxCommandEvent &event);
  void OnResetDictionary(wxCommandEvent &event);
  void OnOk(wxCommandEvent &event);
  void OnItemActivated(wxDataViewEvent &event);
  bool ImportFixturesDictionary();
  bool ImportTrussesDictionary();
  void UpdatePageActionState();
  bool ExportFixturesDictionary();
  bool ExportTrussesDictionary();
  bool ExportFixturesPortableBundle();
  bool ExportTrussesPortableBundle();
  bool ResetFixturesDictionaryToDefault();
  bool ResetTrussesDictionaryToDefault();

  wxNotebook *notebook = nullptr;
  wxDataViewListCtrl *fixtureTable = nullptr;
  wxDataViewListCtrl *trussTable = nullptr;
  ColorfulDataViewListStore *fixtureStore = nullptr;
  ColorfulDataViewListStore *trussStore = nullptr;
  wxButton *addBtn = nullptr;
  wxButton *deleteBtn = nullptr;
  wxButton *downloadBtn = nullptr;
  wxButton *importBtn = nullptr;
  wxButton *exportBtn = nullptr;
  wxButton *okBtn = nullptr;
  wxButton *cancelBtn = nullptr;
  DictionarySelectionControls fixturesSelection;
  DictionarySelectionControls trussesSelection;

  std::vector<std::string> fixturePaths;
  std::vector<std::string> trussPaths;
  std::vector<std::string> fixtureSnapshotAtLoad;
  std::vector<std::string> trussSnapshotAtLoad;
  wxString activeFixtureHoverTooltip;
  wxString activeTrussHoverTooltip;
};
