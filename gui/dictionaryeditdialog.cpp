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
#include "dictionaryeditdialog.h"

#include "columnutils.h"
#include "gdtfdictionary.h"
#include "gdtfloader.h"
#include "mainwindow.h"
#include "projectutils.h"
#include "trussdictionary.h"

#include <algorithm>
#include <filesystem>
#include <vector>

#include <wx/filename.h>

namespace {
struct FixtureRow {
  std::string name;
  std::string path;
  std::string mode;
};

struct TrussRow {
  std::string name;
  std::string path;
};

std::string CopyToLibrary(const std::string &path, const char *libraryName) {
  if (path.empty())
    return {};
  std::filesystem::path src = std::filesystem::u8path(path);
  if (!std::filesystem::exists(src))
    return {};
  std::filesystem::path dir =
      std::filesystem::u8path(ProjectUtils::GetDefaultLibraryPath(libraryName));
  if (dir.empty())
    return path;
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  std::filesystem::path dest = dir / src.filename();
  if (src != dest) {
    std::filesystem::copy_file(src, dest,
                               std::filesystem::copy_options::overwrite_existing,
                               ec);
  }
  if (std::filesystem::exists(dest))
    return dest.string();
  return path;
}

std::vector<std::string> GetSortedModes(const std::string &path) {
  auto modes = GetGdtfModes(path);
  if (modes.size() <= 1)
    return modes;
  std::sort(modes.begin(), modes.end());
  return modes;
}

void SortFixtureRows(std::vector<FixtureRow> &rows) {
  std::sort(rows.begin(), rows.end(), [](const FixtureRow &a, const FixtureRow &b) {
    return a.name < b.name;
  });
}

void SortTrussRows(std::vector<TrussRow> &rows) {
  std::sort(rows.begin(), rows.end(), [](const TrussRow &a, const TrussRow &b) {
    return a.name < b.name;
  });
}
} // namespace

DictionaryEditDialog::DictionaryEditDialog(wxWindow *parent)
    : wxDialog(parent, wxID_ANY, "Dictionary editor", wxDefaultPosition,
               wxSize(760, 520), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) {
  BuildLayout();
  LoadFixtures();
  LoadTrusses();
}

void DictionaryEditDialog::BuildLayout() {
  wxBoxSizer *topSizer = new wxBoxSizer(wxVERTICAL);
  notebook = new wxNotebook(this, wxID_ANY);

  wxPanel *fixturePanel = new wxPanel(notebook);
  wxBoxSizer *fixtureSizer = new wxBoxSizer(wxVERTICAL);
  fixtureTable = new wxDataViewListCtrl(fixturePanel, wxID_ANY, wxDefaultPosition,
                                        wxDefaultSize, wxDV_ROW_LINES);
  int flags = wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE;
  fixtureTable->AppendTextColumn("Name", wxDATAVIEW_CELL_EDITABLE, 200,
                                 wxALIGN_LEFT, flags);
  fixtureTable->AppendTextColumn("File", wxDATAVIEW_CELL_INERT, 260,
                                 wxALIGN_LEFT, flags);
  fixtureTable->AppendTextColumn("Mode", wxDATAVIEW_CELL_INERT, 120,
                                 wxALIGN_LEFT, flags);
  ColumnUtils::EnforceMinColumnWidth(fixtureTable);
  fixtureSizer->Add(fixtureTable, 1, wxEXPAND | wxALL, 8);
  fixturePanel->SetSizer(fixtureSizer);

  wxPanel *trussPanel = new wxPanel(notebook);
  wxBoxSizer *trussSizer = new wxBoxSizer(wxVERTICAL);
  trussTable = new wxDataViewListCtrl(trussPanel, wxID_ANY, wxDefaultPosition,
                                      wxDefaultSize, wxDV_ROW_LINES);
  trussTable->AppendTextColumn("Name", wxDATAVIEW_CELL_EDITABLE, 200,
                               wxALIGN_LEFT, flags);
  trussTable->AppendTextColumn("File", wxDATAVIEW_CELL_INERT, 260,
                               wxALIGN_LEFT, flags);
  ColumnUtils::EnforceMinColumnWidth(trussTable);
  trussSizer->Add(trussTable, 1, wxEXPAND | wxALL, 8);
  trussPanel->SetSizer(trussSizer);

  notebook->AddPage(fixturePanel, "Fixtures");
  notebook->AddPage(trussPanel, "Trusses");

  topSizer->Add(notebook, 1, wxEXPAND | wxALL, 8);

  wxBoxSizer *btnSizer = new wxBoxSizer(wxHORIZONTAL);
  addBtn = new wxButton(this, wxID_ADD, "Add");
  deleteBtn = new wxButton(this, wxID_DELETE, "Delete");
  downloadBtn = new wxButton(this, wxID_ANY, "Download GDTF");
  okBtn = new wxButton(this, wxID_OK, "OK");
  cancelBtn = new wxButton(this, wxID_CANCEL, "Cancel");

  btnSizer->Add(addBtn, 0, wxRIGHT, 5);
  btnSizer->Add(deleteBtn, 0, wxRIGHT, 5);
  btnSizer->Add(downloadBtn, 0, wxRIGHT, 10);
  btnSizer->AddStretchSpacer(1);
  btnSizer->Add(okBtn, 0, wxRIGHT, 5);
  btnSizer->Add(cancelBtn, 0);
  topSizer->Add(btnSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

  SetSizer(topSizer);
  SetMinSize(wxSize(640, 420));

  addBtn->Bind(wxEVT_BUTTON, &DictionaryEditDialog::OnAdd, this);
  deleteBtn->Bind(wxEVT_BUTTON, &DictionaryEditDialog::OnDelete, this);
  downloadBtn->Bind(wxEVT_BUTTON, &DictionaryEditDialog::OnDownloadGdtf, this);
  okBtn->Bind(wxEVT_BUTTON, &DictionaryEditDialog::OnOk, this);
  fixtureTable->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED,
                     &DictionaryEditDialog::OnItemActivated, this);
  trussTable->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED,
                   &DictionaryEditDialog::OnItemActivated, this);
}

bool DictionaryEditDialog::IsFixturesPage() const {
  return notebook->GetSelection() == 0;
}

void DictionaryEditDialog::LoadFixtures() {
  fixtureTable->DeleteAllItems();
  fixturePaths.clear();

  auto dictOpt = GdtfDictionary::Load();
  if (!dictOpt)
    return;

  std::vector<FixtureRow> rows;
  rows.reserve(dictOpt->size());
  for (const auto &[name, entry] : *dictOpt) {
    if (entry.path.empty())
      continue;
    if (!std::filesystem::exists(entry.path))
      continue;
    FixtureRow row{ name, entry.path, entry.mode };
    rows.push_back(row);
  }
  SortFixtureRows(rows);

  fixturePaths.reserve(rows.size());
  for (const auto &row : rows) {
    wxVector<wxVariant> items;
    items.push_back(wxString::FromUTF8(row.name));
    items.push_back(wxString::FromUTF8(std::filesystem::path(row.path).filename().string()));
    items.push_back(wxString::FromUTF8(row.mode));
    fixtureTable->AppendItem(items);
    fixturePaths.push_back(row.path);
  }
}

void DictionaryEditDialog::LoadTrusses() {
  trussTable->DeleteAllItems();
  trussPaths.clear();

  auto dictOpt = TrussDictionary::Load();
  if (!dictOpt)
    return;

  std::vector<TrussRow> rows;
  rows.reserve(dictOpt->size());
  for (const auto &[name, path] : *dictOpt) {
    if (path.empty())
      continue;
    if (!std::filesystem::exists(path))
      continue;
    rows.push_back({name, path});
  }
  SortTrussRows(rows);

  trussPaths.reserve(rows.size());
  for (const auto &row : rows) {
    wxVector<wxVariant> items;
    items.push_back(wxString::FromUTF8(row.name));
    items.push_back(wxString::FromUTF8(std::filesystem::path(row.path).filename().string()));
    trussTable->AppendItem(items);
    trussPaths.push_back(row.path);
  }
}

void DictionaryEditDialog::SaveFixtures() {
  std::vector<FixtureRow> rows;
  int count = fixtureTable->GetItemCount();
  rows.reserve(static_cast<size_t>(count));
  for (int i = 0; i < count; ++i) {
    wxVariant nameVar;
    fixtureTable->GetValue(nameVar, i, 0);
    std::string name = std::string(nameVar.GetString().ToUTF8());
    if (name.empty())
      continue;
    if (static_cast<size_t>(i) >= fixturePaths.size())
      continue;
    const std::string &path = fixturePaths[i];
    if (path.empty())
      continue;
    if (!std::filesystem::exists(path))
      continue;
    wxVariant modeVar;
    fixtureTable->GetValue(modeVar, i, 2);
    std::string mode = std::string(modeVar.GetString().ToUTF8());
    std::string stored = CopyToLibrary(path, "fixtures");
    if (stored.empty())
      continue;
    rows.push_back({name, stored, mode});
  }

  SortFixtureRows(rows);
  std::unordered_map<std::string, GdtfDictionary::Entry> dict;
  dict.reserve(rows.size());
  for (const auto &row : rows)
    dict[row.name] = {row.path, row.mode};
  GdtfDictionary::Save(dict);

  LoadFixtures();
}

void DictionaryEditDialog::SaveTrusses() {
  std::vector<TrussRow> rows;
  int count = trussTable->GetItemCount();
  rows.reserve(static_cast<size_t>(count));
  for (int i = 0; i < count; ++i) {
    wxVariant nameVar;
    trussTable->GetValue(nameVar, i, 0);
    std::string name = std::string(nameVar.GetString().ToUTF8());
    if (name.empty())
      continue;
    if (static_cast<size_t>(i) >= trussPaths.size())
      continue;
    const std::string &path = trussPaths[i];
    if (path.empty())
      continue;
    if (!std::filesystem::exists(path))
      continue;
    std::string stored = CopyToLibrary(path, "trusses");
    if (stored.empty())
      continue;
    rows.push_back({name, stored});
  }

  SortTrussRows(rows);
  std::unordered_map<std::string, std::string> dict;
  dict.reserve(rows.size());
  for (const auto &row : rows)
    dict[row.name] = row.path;
  TrussDictionary::Save(dict);

  LoadTrusses();
}

void DictionaryEditDialog::OnAdd(wxCommandEvent &WXUNUSED(event)) {
  if (IsFixturesPage()) {
    wxString fixDir =
        wxString::FromUTF8(ProjectUtils::GetDefaultLibraryPath("fixtures"));
    wxFileDialog fdlg(this, "Select GDTF file", fixDir, wxEmptyString,
                      "*.gdtf", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (fdlg.ShowModal() != wxID_OK)
      return;
    wxString path = fdlg.GetPath();
    std::string fullPath = std::string(path.ToUTF8());
    std::string name = GetGdtfFixtureName(fullPath);
    if (name.empty())
      name = wxFileName(path).GetName().ToStdString();

    std::string mode;
    auto modes = GetSortedModes(fullPath);
    if (!modes.empty()) {
      wxArrayString choices;
      for (const auto &m : modes)
        choices.push_back(wxString::FromUTF8(m));
      wxSingleChoiceDialog dlg(this, "Select DMX mode", "DMX Mode", choices);
      if (dlg.ShowModal() == wxID_OK)
        mode = std::string(dlg.GetStringSelection().ToUTF8());
    }

    wxVector<wxVariant> items;
    items.push_back(wxString::FromUTF8(name));
    items.push_back(wxString::FromUTF8(std::filesystem::path(fullPath).filename().string()));
    items.push_back(wxString::FromUTF8(mode));
    fixtureTable->AppendItem(items);
    fixturePaths.push_back(fullPath);
  } else {
    wxString trussDir =
        wxString::FromUTF8(ProjectUtils::GetDefaultLibraryPath("trusses"));
    wxFileDialog fdlg(this, "Select Truss file", trussDir, wxEmptyString,
                      "Truss files (*.gtruss;*.3ds;*.glb)|*.gtruss;*.3ds;*.glb|All files|*.*",
                      wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (fdlg.ShowModal() != wxID_OK)
      return;
    wxString path = fdlg.GetPath();
    std::string fullPath = std::string(path.ToUTF8());
    std::string name = wxFileName(path).GetName().ToStdString();

    wxVector<wxVariant> items;
    items.push_back(wxString::FromUTF8(name));
    items.push_back(wxString::FromUTF8(std::filesystem::path(fullPath).filename().string()));
    trussTable->AppendItem(items);
    trussPaths.push_back(fullPath);
  }
}

void DictionaryEditDialog::OnDelete(wxCommandEvent &WXUNUSED(event)) {
  if (IsFixturesPage()) {
    wxDataViewItemArray selections;
    fixtureTable->GetSelections(selections);
    std::vector<int> rows;
    rows.reserve(selections.size());
    for (const auto &item : selections) {
      int row = fixtureTable->ItemToRow(item);
      if (row != wxNOT_FOUND)
        rows.push_back(row);
    }
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    for (int row : rows) {
      fixtureTable->DeleteItem(row);
      if (static_cast<size_t>(row) < fixturePaths.size())
        fixturePaths.erase(fixturePaths.begin() + row);
    }
  } else {
    wxDataViewItemArray selections;
    trussTable->GetSelections(selections);
    std::vector<int> rows;
    rows.reserve(selections.size());
    for (const auto &item : selections) {
      int row = trussTable->ItemToRow(item);
      if (row != wxNOT_FOUND)
        rows.push_back(row);
    }
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    for (int row : rows) {
      trussTable->DeleteItem(row);
      if (static_cast<size_t>(row) < trussPaths.size())
        trussPaths.erase(trussPaths.begin() + row);
    }
  }
}

void DictionaryEditDialog::OnDownloadGdtf(wxCommandEvent &WXUNUSED(event)) {
  if (auto *parent = GetParent()) {
    wxCommandEvent evt(wxEVT_MENU, ID_Tools_DownloadGdtf);
    parent->ProcessWindowEvent(evt);
  }
}

void DictionaryEditDialog::OnOk(wxCommandEvent &WXUNUSED(event)) {
  SaveFixtures();
  SaveTrusses();
  EndModal(wxID_OK);
}

void DictionaryEditDialog::OnItemActivated(wxDataViewEvent &event) {
  wxDataViewListCtrl *table = IsFixturesPage() ? fixtureTable : trussTable;
  if (!table)
    return;
  wxDataViewItem item = event.GetItem();
  int row = table->ItemToRow(item);
  if (row == wxNOT_FOUND)
    return;
  int col = event.GetColumn();
  if (IsFixturesPage()) {
    if (col == 2) {
      if (static_cast<size_t>(row) >= fixturePaths.size())
        return;
      std::string fullPath = fixturePaths[row];
      if (fullPath.empty() || !std::filesystem::exists(fullPath)) {
        table->SetValue(wxVariant(wxString()), row, 2);
        return;
      }
      auto modes = GetSortedModes(fullPath);
      if (modes.empty()) {
        table->SetValue(wxVariant(wxString()), row, 2);
        return;
      }
      wxArrayString choices;
      for (const auto &m : modes)
        choices.push_back(wxString::FromUTF8(m));
      wxSingleChoiceDialog dlg(this, "Select DMX mode", "DMX Mode", choices);
      if (dlg.ShowModal() == wxID_OK) {
        std::string mode = std::string(dlg.GetStringSelection().ToUTF8());
        table->SetValue(wxVariant(wxString::FromUTF8(mode)), row, 2);
      }
      return;
    }
    if (col != 1)
      return;
    wxFileDialog fdlg(this, "Select GDTF file",
                      wxString::FromUTF8(ProjectUtils::GetDefaultLibraryPath("fixtures")),
                      wxEmptyString, "*.gdtf", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (fdlg.ShowModal() != wxID_OK)
      return;
    wxString path = fdlg.GetPath();
    std::string fullPath = std::string(path.ToUTF8());
    if (static_cast<size_t>(row) >= fixturePaths.size())
      fixturePaths.resize(row + 1);
    fixturePaths[row] = fullPath;
    table->SetValue(wxVariant(wxString::FromUTF8(std::filesystem::path(fullPath).filename().string())), row, 1);

    std::string mode;
    auto modes = GetSortedModes(fullPath);
    if (!modes.empty()) {
      wxArrayString choices;
      for (const auto &m : modes)
        choices.push_back(wxString::FromUTF8(m));
      wxSingleChoiceDialog dlg(this, "Select DMX mode", "DMX Mode", choices);
      if (dlg.ShowModal() == wxID_OK)
        mode = std::string(dlg.GetStringSelection().ToUTF8());
    }
    table->SetValue(wxVariant(wxString::FromUTF8(mode)), row, 2);
  } else {
    if (col != 1)
      return;
    wxFileDialog fdlg(this, "Select Truss file",
                      wxString::FromUTF8(ProjectUtils::GetDefaultLibraryPath("trusses")),
                      wxEmptyString,
                      "Truss files (*.gtruss;*.3ds;*.glb)|*.gtruss;*.3ds;*.glb|All files|*.*",
                      wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (fdlg.ShowModal() != wxID_OK)
      return;
    wxString path = fdlg.GetPath();
    std::string fullPath = std::string(path.ToUTF8());
    if (static_cast<size_t>(row) >= trussPaths.size())
      trussPaths.resize(row + 1);
    trussPaths[row] = fullPath;
    table->SetValue(wxVariant(wxString::FromUTF8(std::filesystem::path(fullPath).filename().string())), row, 1);
  }
}
