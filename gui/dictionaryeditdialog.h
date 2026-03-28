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

#include <string>
#include <vector>

class DictionaryEditDialog : public wxDialog {
public:
  explicit DictionaryEditDialog(wxWindow *parent);

private:
  void BuildLayout();
  void LoadFixtures();
  void LoadTrusses();
  void SaveFixtures();
  void SaveTrusses();
  std::vector<std::string> BuildFixtureSnapshotFromUi() const;
  std::vector<std::string> BuildTrussSnapshotFromUi() const;
  bool HasFixtureChanges() const;
  bool HasTrussChanges() const;
  void ShowDictionaryLoadStatusMessages();
  bool IsFixturesPage() const;

  void OnAdd(wxCommandEvent &event);
  void OnDelete(wxCommandEvent &event);
  void OnDownloadGdtf(wxCommandEvent &event);
  void OnImportDictionary(wxCommandEvent &event);
  void OnExportDictionary(wxCommandEvent &event);
  void OnExportPortableBundle(wxCommandEvent &event);
  void OnResetDictionary(wxCommandEvent &event);
  void OnOk(wxCommandEvent &event);
  void OnItemActivated(wxDataViewEvent &event);
  bool ImportFixturesDictionary();
  bool ImportTrussesDictionary();
  bool ExportFixturesDictionary();
  bool ExportTrussesDictionary();
  bool ExportFixturesPortableBundle();
  bool ExportTrussesPortableBundle();
  bool ResetFixturesDictionaryToDefault();
  bool ResetTrussesDictionaryToDefault();

  wxNotebook *notebook = nullptr;
  wxDataViewListCtrl *fixtureTable = nullptr;
  wxDataViewListCtrl *trussTable = nullptr;
  wxButton *addBtn = nullptr;
  wxButton *deleteBtn = nullptr;
  wxButton *downloadBtn = nullptr;
  wxButton *importBtn = nullptr;
  wxButton *exportBtn = nullptr;
  wxButton *exportPortableBtn = nullptr;
  wxButton *resetBtn = nullptr;
  wxButton *okBtn = nullptr;
  wxButton *cancelBtn = nullptr;

  std::vector<std::string> fixturePaths;
  std::vector<std::string> trussPaths;
  std::vector<std::string> fixtureSnapshotAtLoad;
  std::vector<std::string> trussSnapshotAtLoad;
};
