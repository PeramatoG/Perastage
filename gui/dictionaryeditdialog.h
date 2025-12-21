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

class DictionaryEditDialog : public wxDialog {
public:
  explicit DictionaryEditDialog(wxWindow *parent);

private:
  void BuildLayout();
  void LoadFixtures();
  void LoadTrusses();
  void SaveFixtures();
  void SaveTrusses();
  bool IsFixturesPage() const;

  void OnAdd(wxCommandEvent &event);
  void OnDelete(wxCommandEvent &event);
  void OnDownloadGdtf(wxCommandEvent &event);
  void OnOk(wxCommandEvent &event);
  void OnItemActivated(wxDataViewEvent &event);

  wxNotebook *notebook = nullptr;
  wxDataViewListCtrl *fixtureTable = nullptr;
  wxDataViewListCtrl *trussTable = nullptr;
  wxButton *addBtn = nullptr;
  wxButton *deleteBtn = nullptr;
  wxButton *downloadBtn = nullptr;
  wxButton *okBtn = nullptr;
  wxButton *cancelBtn = nullptr;

  std::vector<std::string> fixturePaths;
  std::vector<std::string> trussPaths;
};
