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
#include "dictionary_json_contract.h"
#include "gdtfdictionary.h"
#include "gdtfloader.h"
#include "json.hpp"
#include "mainwindow.h"
#include "projectutils.h"
#include "trussdictionary.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
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

wxString BuildSummaryText(const DictionaryImportSummary &summary) {
  std::ostringstream oss;
  oss << "added_count: " << summary.added_count << "\n";
  oss << "overwritten_count: " << summary.overwritten_count << "\n";
  oss << "skipped_count: " << summary.skipped_count << "\n";
  oss << "errors: " << summary.errors.size();
  if (!summary.errors.empty()) {
    for (const auto &error : summary.errors)
      oss << "\n- " << error;
  }
  return wxString::FromUTF8(oss.str());
}

wxString GetPolicyDescription(DictionaryImportPolicy policy) {
  switch (policy) {
  case DictionaryImportPolicy::AddMissing:
    return "AddMissing: insert only missing keys";
  case DictionaryImportPolicy::AddAndOverwrite:
    return "AddAndOverwrite: insert new keys and overwrite matches";
  case DictionaryImportPolicy::ReplaceAll:
    return "ReplaceAll: discard current dictionary and use imported one";
  }
  return "Unknown policy";
}

bool AskImportPolicy(wxWindow *parent, DictionaryImportPolicy &policyOut) {
  wxArrayString choices;
  choices.push_back("AddMissing");
  choices.push_back("AddAndOverwrite");
  choices.push_back("ReplaceAll");
  wxSingleChoiceDialog policyDlg(parent, "Select import policy",
                                 "Dictionary import", choices);
  if (policyDlg.ShowModal() != wxID_OK)
    return false;
  const int selection = policyDlg.GetSelection();
  if (selection == 0)
    policyOut = DictionaryImportPolicy::AddMissing;
  else if (selection == 1)
    policyOut = DictionaryImportPolicy::AddAndOverwrite;
  else
    policyOut = DictionaryImportPolicy::ReplaceAll;
  return true;
}

bool ConfirmReplaceAllOperation(wxWindow *parent, const wxString &dictionaryName) {
  const wxString message =
      "This will replace all current entries in " + dictionaryName +
      " dictionary.\nThis action is destructive.\n\nContinue?";
  return wxMessageBox(message, "Confirm replace all",
                      wxYES_NO | wxNO_DEFAULT | wxICON_WARNING,
                      parent) == wxYES;
}

bool SaveFixturesSnapshotToFile(const std::string &outputPath,
                                const std::unordered_map<std::string, GdtfDictionary::Entry> &dict) {
  std::vector<std::string> keys;
  keys.reserve(dict.size());
  for (const auto &[name, _] : dict)
    keys.push_back(name);
  std::sort(keys.begin(), keys.end());

  nlohmann::json entries = nlohmann::json::object();
  for (const auto &name : keys) {
    const auto &entry = dict.at(name);
    if (entry.path.empty() && entry.mode.empty() && entry.category.empty())
      continue;
    nlohmann::json obj;
    if (!entry.path.empty()) {
      const std::string fileName = std::filesystem::path(entry.path).filename().string();
      if (!fileName.empty())
        obj["file"] = fileName;
    }
    if (!entry.mode.empty())
      obj["mode"] = entry.mode;
    if (!entry.category.empty())
      obj["category"] = entry.category;
    if (!obj.empty())
      entries[name] = obj;
  }

  const nlohmann::json root =
      DictionaryJsonContract::MakeRoot("fixtures", std::move(entries));
  std::ofstream out(outputPath);
  if (!out.is_open())
    return false;
  out << root.dump(4);
  return true;
}

bool SaveTrussesSnapshotToFile(const std::string &outputPath,
                               const std::unordered_map<std::string, std::string> &dict) {
  std::vector<std::string> keys;
  keys.reserve(dict.size());
  for (const auto &[name, _] : dict)
    keys.push_back(name);
  std::sort(keys.begin(), keys.end());

  nlohmann::json entries = nlohmann::json::object();
  for (const auto &name : keys) {
    const auto &path = dict.at(name);
    if (path.empty())
      continue;
    const std::string fileName = std::filesystem::path(path).filename().string();
    if (fileName.empty())
      continue;
    entries[name] = nlohmann::json{ {"file", fileName} };
  }

  const nlohmann::json root =
      DictionaryJsonContract::MakeRoot("trusses", std::move(entries));
  std::ofstream out(outputPath);
  if (!out.is_open())
    return false;
  out << root.dump(4);
  return true;
}
} // namespace

DictionaryEditDialog::DictionaryEditDialog(wxWindow *parent)
    : wxDialog(parent, wxID_ANY, "Dictionary editor", wxDefaultPosition,
               wxSize(760, 520), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) {
  BuildLayout();
  LoadFixtures();
  LoadTrusses();
  ShowDictionaryLoadStatusMessages();
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
  importBtn = new wxButton(this, wxID_ANY, "Import dictionary...");
  exportBtn = new wxButton(this, wxID_ANY, "Export dictionary...");
  resetBtn = new wxButton(this, wxID_ANY, "Reset to default");
  okBtn = new wxButton(this, wxID_OK, "OK");
  cancelBtn = new wxButton(this, wxID_CANCEL, "Cancel");

  btnSizer->Add(addBtn, 0, wxRIGHT, 5);
  btnSizer->Add(deleteBtn, 0, wxRIGHT, 5);
  btnSizer->Add(downloadBtn, 0, wxRIGHT, 10);
  btnSizer->Add(importBtn, 0, wxRIGHT, 10);
  btnSizer->Add(exportBtn, 0, wxRIGHT, 5);
  btnSizer->Add(resetBtn, 0, wxRIGHT, 10);
  btnSizer->AddStretchSpacer(1);
  btnSizer->Add(okBtn, 0, wxRIGHT, 5);
  btnSizer->Add(cancelBtn, 0);
  topSizer->Add(btnSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

  SetSizer(topSizer);
  SetMinSize(wxSize(920, 420));

  addBtn->Bind(wxEVT_BUTTON, &DictionaryEditDialog::OnAdd, this);
  deleteBtn->Bind(wxEVT_BUTTON, &DictionaryEditDialog::OnDelete, this);
  downloadBtn->Bind(wxEVT_BUTTON, &DictionaryEditDialog::OnDownloadGdtf, this);
  importBtn->Bind(wxEVT_BUTTON, &DictionaryEditDialog::OnImportDictionary, this);
  exportBtn->Bind(wxEVT_BUTTON, &DictionaryEditDialog::OnExportDictionary, this);
  resetBtn->Bind(wxEVT_BUTTON, &DictionaryEditDialog::OnResetDictionary, this);
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

void DictionaryEditDialog::ShowDictionaryLoadStatusMessages() {
  bool shouldShowFallbackMessage = false;
  std::string errorMessage;

  const GdtfDictionary::LoadStatus fixturesStatus =
      GdtfDictionary::GetLastLoadStatus();
  if (fixturesStatus.usedDefaultDictionary)
    shouldShowFallbackMessage = true;
  if (!fixturesStatus.error.empty())
    errorMessage = fixturesStatus.error;

  const TrussDictionary::LoadStatus trussStatus =
      TrussDictionary::GetLastLoadStatus();
  if (trussStatus.usedDefaultDictionary)
    shouldShowFallbackMessage = true;
  if (errorMessage.empty() && !trussStatus.error.empty())
    errorMessage = trussStatus.error;

  if (!errorMessage.empty()) {
    wxMessageBox(
        wxString::FromUTF8(errorMessage),
        "Dictionary load error",
        wxICON_ERROR | wxOK,
        this);
    return;
  }

  if (shouldShowFallbackMessage) {
    wxMessageBox(
        "Se cargó diccionario por defecto debido a error en el archivo de usuario",
        "Dictionary warning",
        wxICON_WARNING | wxOK,
        this);
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
    dict[row.name] = {row.path, row.mode, ""};
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
                      "Truss files (*.gdtf;*.gtruss;*.3ds;*.glb)|*.gdtf;*.gtruss;*.3ds;*.glb|All files|*.*",
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

void DictionaryEditDialog::OnImportDictionary(wxCommandEvent &WXUNUSED(event)) {
  if (IsFixturesPage()) {
    (void)ImportFixturesDictionary();
    return;
  }
  (void)ImportTrussesDictionary();
}

bool DictionaryEditDialog::ImportFixturesDictionary() {
  wxFileDialog fileDialog(this, "Import fixtures dictionary", wxEmptyString,
                          wxEmptyString, "JSON files (*.json)|*.json",
                          wxFD_OPEN | wxFD_FILE_MUST_EXIST);
  if (fileDialog.ShowModal() != wxID_OK)
    return false;
  const std::string importPath = std::string(fileDialog.GetPath().ToUTF8());
  const auto validationPreview =
      GdtfDictionary::PreviewImportFromFile(importPath, DictionaryImportPolicy::AddMissing);
  if (validationPreview.HasErrors()) {
    wxMessageBox("Invalid fixtures dictionary file.\n\n" +
                     BuildSummaryText(validationPreview),
                 "Import fixtures dictionary", wxOK | wxICON_ERROR, this);
    return false;
  }

  DictionaryImportPolicy policy = DictionaryImportPolicy::AddMissing;
  if (!AskImportPolicy(this, policy))
    return false;
  if (policy == DictionaryImportPolicy::ReplaceAll &&
      !ConfirmReplaceAllOperation(this, "fixtures")) {
    return false;
  }

  const auto preview = GdtfDictionary::PreviewImportFromFile(importPath, policy);
  wxString confirmText = "Policy:\n" + GetPolicyDescription(policy) +
                         "\n\nPreview summary:\n" + BuildSummaryText(preview) +
                         "\n\nApply import?";
  if (wxMessageBox(confirmText, "Confirm fixtures dictionary import",
                   wxYES_NO | wxICON_QUESTION, this) != wxYES) {
    return false;
  }

  const auto result = GdtfDictionary::ApplyImportFromFile(importPath, policy);
  LoadFixtures();
  wxMessageBox("Fixtures dictionary import completed.\n\n" +
                   BuildSummaryText(result),
               "Fixtures dictionary import", wxOK | wxICON_INFORMATION, this);
  return !result.HasErrors();
}

bool DictionaryEditDialog::ImportTrussesDictionary() {
  wxFileDialog fileDialog(this, "Import trusses dictionary", wxEmptyString,
                          wxEmptyString, "JSON files (*.json)|*.json",
                          wxFD_OPEN | wxFD_FILE_MUST_EXIST);
  if (fileDialog.ShowModal() != wxID_OK)
    return false;
  const std::string importPath = std::string(fileDialog.GetPath().ToUTF8());
  const auto validationPreview =
      TrussDictionary::PreviewImportFromFile(importPath, DictionaryImportPolicy::AddMissing);
  if (validationPreview.HasErrors()) {
    wxMessageBox("Invalid trusses dictionary file.\n\n" +
                     BuildSummaryText(validationPreview),
                 "Import trusses dictionary", wxOK | wxICON_ERROR, this);
    return false;
  }

  DictionaryImportPolicy policy = DictionaryImportPolicy::AddMissing;
  if (!AskImportPolicy(this, policy))
    return false;
  if (policy == DictionaryImportPolicy::ReplaceAll &&
      !ConfirmReplaceAllOperation(this, "trusses")) {
    return false;
  }

  const auto preview = TrussDictionary::PreviewImportFromFile(importPath, policy);
  wxString confirmText = "Policy:\n" + GetPolicyDescription(policy) +
                         "\n\nPreview summary:\n" + BuildSummaryText(preview) +
                         "\n\nApply import?";
  if (wxMessageBox(confirmText, "Confirm trusses dictionary import",
                   wxYES_NO | wxICON_QUESTION, this) != wxYES) {
    return false;
  }

  const auto result = TrussDictionary::ApplyImportFromFile(importPath, policy);
  LoadTrusses();
  wxMessageBox("Trusses dictionary import completed.\n\n" +
                   BuildSummaryText(result),
               "Trusses dictionary import", wxOK | wxICON_INFORMATION, this);
  return !result.HasErrors();
}

void DictionaryEditDialog::OnExportDictionary(wxCommandEvent &WXUNUSED(event)) {
  if (IsFixturesPage()) {
    (void)ExportFixturesDictionary();
    return;
  }
  (void)ExportTrussesDictionary();
}

void DictionaryEditDialog::OnResetDictionary(wxCommandEvent &WXUNUSED(event)) {
  if (IsFixturesPage()) {
    (void)ResetFixturesDictionaryToDefault();
    return;
  }
  (void)ResetTrussesDictionaryToDefault();
}

bool DictionaryEditDialog::ExportFixturesDictionary() {
  auto dictOpt = GdtfDictionary::Load();
  if (!dictOpt) {
    wxMessageBox("Could not load fixtures dictionary for export.",
                 "Export fixtures dictionary", wxICON_ERROR | wxOK, this);
    return false;
  }

  wxFileDialog fileDialog(this, "Export fixtures dictionary", wxEmptyString,
                          "gdtf_dictionary_snapshot.json",
                          "JSON files (*.json)|*.json",
                          wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if (fileDialog.ShowModal() != wxID_OK)
    return false;

  const std::string outputPath = std::string(fileDialog.GetPath().ToUTF8());
  if (!SaveFixturesSnapshotToFile(outputPath, *dictOpt)) {
    wxMessageBox("Could not write fixtures dictionary snapshot.",
                 "Export fixtures dictionary", wxICON_ERROR | wxOK, this);
    return false;
  }
  wxMessageBox("Fixtures dictionary snapshot exported successfully.",
               "Export fixtures dictionary", wxICON_INFORMATION | wxOK, this);
  return true;
}

bool DictionaryEditDialog::ExportTrussesDictionary() {
  auto dictOpt = TrussDictionary::Load();
  if (!dictOpt) {
    wxMessageBox("Could not load trusses dictionary for export.",
                 "Export trusses dictionary", wxICON_ERROR | wxOK, this);
    return false;
  }

  wxFileDialog fileDialog(this, "Export trusses dictionary", wxEmptyString,
                          "truss_dictionary_snapshot.json",
                          "JSON files (*.json)|*.json",
                          wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if (fileDialog.ShowModal() != wxID_OK)
    return false;

  const std::string outputPath = std::string(fileDialog.GetPath().ToUTF8());
  if (!SaveTrussesSnapshotToFile(outputPath, *dictOpt)) {
    wxMessageBox("Could not write trusses dictionary snapshot.",
                 "Export trusses dictionary", wxICON_ERROR | wxOK, this);
    return false;
  }
  wxMessageBox("Trusses dictionary snapshot exported successfully.",
               "Export trusses dictionary", wxICON_INFORMATION | wxOK, this);
  return true;
}

bool DictionaryEditDialog::ResetFixturesDictionaryToDefault() {
  if (wxMessageBox(
          "Reset fixtures dictionary to application defaults?\n"
          "Current entries will be replaced.",
          "Reset fixtures dictionary",
          wxYES_NO | wxNO_DEFAULT | wxICON_WARNING,
          this) != wxYES) {
    return false;
  }

  const std::filesystem::path basePath =
      ProjectUtils::GetBaseLibraryPath("fixtures") / "gdtf_dictionary.json";
  const auto result = GdtfDictionary::ApplyImportFromFile(
      basePath.string(), DictionaryImportPolicy::ReplaceAll);
  LoadFixtures();
  wxMessageBox("Fixtures dictionary restored to defaults.\n\n" +
                   BuildSummaryText(result),
               "Reset fixtures dictionary", wxICON_INFORMATION | wxOK, this);
  return !result.HasErrors();
}

bool DictionaryEditDialog::ResetTrussesDictionaryToDefault() {
  if (wxMessageBox(
          "Reset trusses dictionary to application defaults?\n"
          "Current entries will be replaced.",
          "Reset trusses dictionary",
          wxYES_NO | wxNO_DEFAULT | wxICON_WARNING,
          this) != wxYES) {
    return false;
  }

  const std::filesystem::path basePath =
      ProjectUtils::GetBaseLibraryPath("trusses") / "truss_dictionary.json";
  const auto result = TrussDictionary::ApplyImportFromFile(
      basePath.string(), DictionaryImportPolicy::ReplaceAll);
  LoadTrusses();
  wxMessageBox("Trusses dictionary restored to defaults.\n\n" +
                   BuildSummaryText(result),
               "Reset trusses dictionary", wxICON_INFORMATION | wxOK, this);
  return !result.HasErrors();
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
                      "Truss files (*.gdtf;*.gtruss;*.3ds;*.glb)|*.gdtf;*.gtruss;*.3ds;*.glb|All files|*.*",
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
