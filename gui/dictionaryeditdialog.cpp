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
#include "dictionary_bundle.h"
#include "dictionary_json_contract.h"
#include "file_import_utils.h"
#include "gdtfdictionary.h"
#include "gdtfloader.h"
#include "json.hpp"
#include "mainwindow.h"
#include "projectutils.h"
#include "trussdictionary.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <vector>

#include <wx/filename.h>
#include <wx/busyinfo.h>

namespace {
struct FixtureRow {
  std::string name;
  std::string path;
  std::string mode;
  std::string source;
  std::string sha256;
};

struct TrussRow {
  std::string name;
  std::string path;
};

struct CopiedLibraryAsset {
  std::string path;
  std::string source;
  std::string sha256;
};

struct ExportPathStatusSummary {
  size_t total_entries = 0;
  size_t found_entries = 0;
  size_t missing_entries = 0;
};

struct SnapshotExportResult {
  bool success = false;
  size_t copied_assets = 0;
  size_t missing_assets = 0;
  std::vector<std::string> copy_errors;
};

struct ImportPathValidationSummary {
  size_t checked_entries = 0;
  size_t found_entries = 0;
  size_t missing_entries = 0;
  std::vector<std::string> missing_examples;
};

bool HasExistingPath(const std::filesystem::path &candidate) {
  if (candidate.empty())
    return false;
  std::error_code ec;
  return std::filesystem::exists(candidate, ec);
}

std::filesystem::path ResolveImportRelativePath(
    const std::filesystem::path &jsonPath, const std::filesystem::path &rawPath) {
  if (rawPath.is_absolute())
    return rawPath;

  const std::filesystem::path importDir = jsonPath.parent_path();
  const std::filesystem::path directPath = importDir / rawPath;
  if (HasExistingPath(directPath))
    return directPath;

  const std::filesystem::path snapshotAssetsPath =
      importDir / (jsonPath.stem().string() + "_assets") / rawPath;
  if (HasExistingPath(snapshotAssetsPath))
    return snapshotAssetsPath;
  return directPath;
}

void RegisterMissingExample(ImportPathValidationSummary &summary,
                            const std::string &entryName,
                            const std::string &path) {
  constexpr size_t kMaxMissingExamples = 5;
  if (summary.missing_examples.size() >= kMaxMissingExamples)
    return;
  summary.missing_examples.push_back(entryName + " -> " + path);
}

ExportPathStatusSummary AnalyzeFixtureExportPaths(
    const std::unordered_map<std::string, GdtfDictionary::Entry> &dict) {
  ExportPathStatusSummary summary;
  summary.total_entries = dict.size();
  for (const auto &[_, entry] : dict) {
    if (HasExistingPath(std::filesystem::u8path(entry.path)))
      ++summary.found_entries;
    else
      ++summary.missing_entries;
  }
  return summary;
}

ExportPathStatusSummary AnalyzeTrussExportPaths(
    const std::unordered_map<std::string, std::string> &dict) {
  ExportPathStatusSummary summary;
  summary.total_entries = dict.size();
  for (const auto &[_, path] : dict) {
    if (HasExistingPath(std::filesystem::u8path(path)))
      ++summary.found_entries;
    else
      ++summary.missing_entries;
  }
  return summary;
}

bool ConfirmExportReferences(wxWindow *parent, const wxString &title,
                             const ExportPathStatusSummary &summary) {
  wxMessageDialog confirmDialog(
      parent,
      "Found file references in the loaded dictionary.\n\n"
      "Total entries: " + wxString::Format("%zu", summary.total_entries) + "\n" +
          "Entries with file found: " +
          wxString::Format("%zu", summary.found_entries) + "\n" +
          "Entries with missing file: " +
          wxString::Format("%zu", summary.missing_entries) +
          "\n\nDo you want to export references only?",
      title, wxOK | wxCANCEL | wxICON_WARNING);
  confirmDialog.SetOKCancelLabels("Export references only", "Cancel");
  return confirmDialog.ShowModal() == wxID_OK;
}

bool AskCopyReferencedAssets(wxWindow *parent, const wxString &title,
                             bool &copyAssetsOut) {
  wxDialog optionsDialog(parent, wxID_ANY, title);
  wxBoxSizer *topSizer = new wxBoxSizer(wxVERTICAL);
  wxCheckBox *copyAssetsCheckbox =
      new wxCheckBox(&optionsDialog, wxID_ANY,
                     "Copy referenced files too");
  copyAssetsCheckbox->SetValue(false);

  topSizer->Add(
      new wxStaticText(
          &optionsDialog, wxID_ANY,
          "Exporting without copying files keeps the previous behavior."),
      0, wxALL, 10);
  topSizer->Add(copyAssetsCheckbox, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);
  topSizer->Add(optionsDialog.CreateSeparatedButtonSizer(wxOK | wxCANCEL), 0,
                wxEXPAND | wxALL, 10);
  optionsDialog.SetSizerAndFit(topSizer);

  if (optionsDialog.ShowModal() != wxID_OK)
    return false;
  copyAssetsOut = copyAssetsCheckbox->GetValue();
  return true;
}

std::filesystem::path BuildSnapshotAssetsDir(
    const std::filesystem::path &snapshotPath) {
  return snapshotPath.parent_path() /
         (snapshotPath.stem().string() + "_assets");
}

std::optional<std::string> CopySnapshotAsset(
    const std::filesystem::path &sourcePath,
    const std::filesystem::path &targetAssetsDir) {
  if (!HasExistingPath(sourcePath))
    return std::nullopt;

  std::error_code ec;
  std::filesystem::create_directories(targetAssetsDir, ec);
  if (ec)
    return std::nullopt;

  const std::string fileName = sourcePath.filename().string();
  if (fileName.empty())
    return std::nullopt;
  const std::filesystem::path destPath = targetAssetsDir / fileName;
  std::filesystem::copy_file(sourcePath, destPath,
                             std::filesystem::copy_options::overwrite_existing, ec);
  if (ec)
    return std::nullopt;
  return std::string("assets/") + fileName;
}

std::optional<nlohmann::json> LoadJsonFromFile(const std::string &filePath,
                                               std::string &error) {
  std::ifstream in(filePath);
  if (!in.is_open()) {
    error = "Could not open import file";
    return std::nullopt;
  }

  nlohmann::json root;
  try {
    in >> root;
  } catch (const std::exception &e) {
    error = std::string("Failed to parse JSON: ") + e.what();
    return std::nullopt;
  }
  return root;
}

ImportPathValidationSummary ValidateFixtureImportPaths(const std::string &importPath) {
  ImportPathValidationSummary summary;
  std::string loadError;
  auto rootOpt = LoadJsonFromFile(importPath, loadError);
  if (!rootOpt)
    return summary;

  std::string parseError;
  auto entriesOpt =
      DictionaryJsonContract::GetEntriesForType(*rootOpt, "fixtures", parseError);
  if (!entriesOpt)
    return summary;

  const std::filesystem::path importFilePath = std::filesystem::u8path(importPath);
  const std::filesystem::path libraryDir = std::filesystem::u8path(
      ProjectUtils::GetDefaultLibraryPath("fixtures"));

  auto checkEntry = [&](const std::string &entryName, const nlohmann::json &entryJson) {
    if (!entryJson.is_object())
      return;
    std::string rawPath;
    if (entryJson.contains("file") && entryJson["file"].is_string())
      rawPath = entryJson["file"].get<std::string>();
    else if (entryJson.contains("path") && entryJson["path"].is_string())
      rawPath = entryJson["path"].get<std::string>();
    if (rawPath.empty())
      return;

    ++summary.checked_entries;
    const std::filesystem::path sourcePath = std::filesystem::u8path(rawPath);
    const std::filesystem::path importRelativePath =
        ResolveImportRelativePath(importFilePath, sourcePath);
    const std::filesystem::path libraryRelativePath = libraryDir / sourcePath.filename();
    if (HasExistingPath(importRelativePath) || HasExistingPath(libraryRelativePath)) {
      ++summary.found_entries;
      return;
    }

    ++summary.missing_entries;
    RegisterMissingExample(summary, entryName, rawPath);
  };

  const auto &entries = **entriesOpt;
  if (entries.is_object()) {
    for (auto it = entries.begin(); it != entries.end(); ++it)
      checkEntry(it.key(), it.value());
  } else if (entries.is_array()) {
    for (size_t i = 0; i < entries.size(); ++i) {
      const auto &entryJson = entries[i];
      if (!entryJson.is_object() || !entryJson.contains("name") ||
          !entryJson["name"].is_string()) {
        continue;
      }
      checkEntry(entryJson["name"].get<std::string>(), entryJson);
    }
  }
  return summary;
}

ImportPathValidationSummary ValidateTrussImportPaths(const std::string &importPath) {
  ImportPathValidationSummary summary;
  std::string loadError;
  auto rootOpt = LoadJsonFromFile(importPath, loadError);
  if (!rootOpt)
    return summary;

  std::string parseError;
  auto entriesOpt =
      DictionaryJsonContract::GetEntriesForType(*rootOpt, "trusses", parseError);
  if (!entriesOpt)
    return summary;

  const std::filesystem::path importFilePath = std::filesystem::u8path(importPath);
  const std::filesystem::path libraryDir =
      std::filesystem::u8path(ProjectUtils::GetDefaultLibraryPath("trusses"));

  auto checkEntry = [&](const std::string &entryName, const nlohmann::json &entryJson) {
    if (!entryJson.is_object())
      return;
    std::string rawPath;
    if (entryJson.contains("file") && entryJson["file"].is_string())
      rawPath = entryJson["file"].get<std::string>();
    else if (entryJson.contains("path") && entryJson["path"].is_string())
      rawPath = entryJson["path"].get<std::string>();
    if (rawPath.empty())
      return;

    ++summary.checked_entries;
    const std::filesystem::path sourcePath = std::filesystem::u8path(rawPath);
    const std::filesystem::path importRelativePath =
        ResolveImportRelativePath(importFilePath, sourcePath);
    const std::filesystem::path libraryRelativePath = libraryDir / sourcePath.filename();
    if (HasExistingPath(importRelativePath) || HasExistingPath(libraryRelativePath)) {
      ++summary.found_entries;
      return;
    }

    ++summary.missing_entries;
    RegisterMissingExample(summary, entryName, rawPath);
  };

  const auto &entries = **entriesOpt;
  if (entries.is_object()) {
    for (auto it = entries.begin(); it != entries.end(); ++it)
      checkEntry(it.key(), it.value());
  } else if (entries.is_array()) {
    for (size_t i = 0; i < entries.size(); ++i) {
      const auto &entryJson = entries[i];
      if (!entryJson.is_object() || !entryJson.contains("name") ||
          !entryJson["name"].is_string()) {
        continue;
      }
      checkEntry(entryJson["name"].get<std::string>(), entryJson);
    }
  }
  return summary;
}

bool ConfirmImportMissingPaths(wxWindow *parent, const wxString &title,
                               const ImportPathValidationSummary &summary) {
  if (summary.missing_entries == 0)
    return true;

  std::ostringstream oss;
  oss << "Some imported file references could not be resolved before applying:\n\n";
  oss << "checked_entries: " << summary.checked_entries << "\n";
  oss << "found_entries: " << summary.found_entries << "\n";
  oss << "missing_entries: " << summary.missing_entries;
  if (!summary.missing_examples.empty()) {
    oss << "\n\nMissing examples:";
    for (const auto &example : summary.missing_examples)
      oss << "\n- " << example;
  }
  oss << "\n\nContinue with import anyway?";

  return wxMessageBox(wxString::FromUTF8(oss.str()), title,
                      wxYES_NO | wxNO_DEFAULT | wxICON_WARNING,
                      parent) == wxYES;
}

std::optional<FileImportUtils::ConflictPolicy>
AskConflictPolicy(wxWindow *parent, const std::filesystem::path &sourcePath,
                  const std::filesystem::path &destPath) {
  wxArrayString choices;
  choices.push_back("Rename (stable: <basename>_<hash>.<ext>)");
  choices.push_back("Overwrite existing file");
  choices.push_back("Cancel");

  wxSingleChoiceDialog dialog(
      parent,
      "The destination file already exists with different content.\n\n"
      "Source: " + wxString::FromUTF8(sourcePath.string()) + "\n"
      "Destination: " + wxString::FromUTF8(destPath.string()) +
          "\n\nChoose conflict policy:",
      "File conflict", choices);

  if (dialog.ShowModal() != wxID_OK)
    return std::nullopt;

  if (dialog.GetSelection() == 0)
    return FileImportUtils::ConflictPolicy::Rename;
  if (dialog.GetSelection() == 1)
    return FileImportUtils::ConflictPolicy::Overwrite;
  return FileImportUtils::ConflictPolicy::Cancel;
}

std::optional<CopiedLibraryAsset>
CopyToLibrary(wxWindow *parent, const std::string &path, const char *libraryName) {
  if (path.empty())
    return std::nullopt;

  const std::filesystem::path src = std::filesystem::u8path(path);
  if (!std::filesystem::exists(src))
    return std::nullopt;

  const std::filesystem::path dir =
      std::filesystem::u8path(ProjectUtils::GetDefaultLibraryPath(libraryName));
  if (dir.empty())
    return CopiedLibraryAsset{path, path, {}};

  const std::filesystem::path dest = dir / src.filename();
  FileImportUtils::ConflictPolicy policy = FileImportUtils::ConflictPolicy::Overwrite;

  std::error_code ec;
  if (std::filesystem::exists(dest, ec) && !ec) {
    const auto srcHash = FileImportUtils::ComputeFileSha256(src);
    const auto dstHash = FileImportUtils::ComputeFileSha256(dest);
    if (srcHash && dstHash && *srcHash != *dstHash) {
      auto chosenPolicy = AskConflictPolicy(parent, src, dest);
      if (!chosenPolicy || *chosenPolicy == FileImportUtils::ConflictPolicy::Cancel)
        return std::nullopt;
      policy = *chosenPolicy;
    }
  }

  const auto copyResult = FileImportUtils::CopyWithConflictPolicy(src, dest, policy);
  if (!copyResult.success)
    return std::nullopt;

  return CopiedLibraryAsset{copyResult.finalPath.string(), src.string(),
                            copyResult.finalSha256};
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
  oss << "missing_files_count: " << summary.missing_files_count << "\n";
  if (!summary.missing_file_examples.empty()) {
    oss << "missing_file_examples:";
    for (const auto &example : summary.missing_file_examples)
      oss << "\n- " << example;
    oss << "\n";
  }
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

bool SaveFixturesSnapshotToFile(
    const std::string &outputPath,
    const std::unordered_map<std::string, GdtfDictionary::Entry> &dict,
    bool copyReferencedAssets, SnapshotExportResult &exportResult) {
  std::vector<std::string> keys;
  keys.reserve(dict.size());
  for (const auto &[name, _] : dict)
    keys.push_back(name);
  std::sort(keys.begin(), keys.end());

  const std::filesystem::path outputFsPath = std::filesystem::u8path(outputPath);
  const std::filesystem::path targetAssetsDir =
      BuildSnapshotAssetsDir(outputFsPath) / "assets";

  nlohmann::json entries = nlohmann::json::object();
  for (const auto &name : keys) {
    const auto &entry = dict.at(name);
    if (entry.path.empty() && entry.mode.empty() && entry.category.empty())
      continue;
    nlohmann::json obj;
    if (!entry.path.empty()) {
      const std::filesystem::path sourcePath = std::filesystem::u8path(entry.path);
      if (copyReferencedAssets) {
        const auto copiedRelativePath = CopySnapshotAsset(sourcePath, targetAssetsDir);
        if (copiedRelativePath) {
          obj["file"] = *copiedRelativePath;
          ++exportResult.copied_assets;
        } else {
          ++exportResult.missing_assets;
          exportResult.copy_errors.push_back(name + " -> " + entry.path);
        }
      } else {
        const std::string fileName = sourcePath.filename().string();
        if (!fileName.empty())
          obj["file"] = fileName;
      }
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
  exportResult.success = true;
  return true;
}

bool SaveTrussesSnapshotToFile(const std::string &outputPath,
                               const std::unordered_map<std::string, std::string> &dict,
                               bool copyReferencedAssets,
                               SnapshotExportResult &exportResult) {
  std::vector<std::string> keys;
  keys.reserve(dict.size());
  for (const auto &[name, _] : dict)
    keys.push_back(name);
  std::sort(keys.begin(), keys.end());

  const std::filesystem::path outputFsPath = std::filesystem::u8path(outputPath);
  const std::filesystem::path targetAssetsDir =
      BuildSnapshotAssetsDir(outputFsPath) / "assets";

  nlohmann::json entries = nlohmann::json::object();
  for (const auto &name : keys) {
    const auto &path = dict.at(name);
    if (path.empty())
      continue;
    const std::filesystem::path sourcePath = std::filesystem::u8path(path);
    if (copyReferencedAssets) {
      const auto copiedRelativePath = CopySnapshotAsset(sourcePath, targetAssetsDir);
      if (copiedRelativePath) {
        entries[name] = nlohmann::json{{"file", *copiedRelativePath}};
        ++exportResult.copied_assets;
      } else {
        ++exportResult.missing_assets;
        exportResult.copy_errors.push_back(name + " -> " + path);
      }
      continue;
    }
    const std::string fileName = sourcePath.filename().string();
    if (fileName.empty())
      continue;
    entries[name] = nlohmann::json{{"file", fileName}};
  }

  const nlohmann::json root =
      DictionaryJsonContract::MakeRoot("trusses", std::move(entries));
  std::ofstream out(outputPath);
  if (!out.is_open())
    return false;
  out << root.dump(4);
  exportResult.success = true;
  return true;
}
} // namespace

DictionaryEditDialog::DictionaryEditDialog(wxWindow *parent)
    : wxDialog(parent, wxID_ANY, "Dictionary editor", wxDefaultPosition,
               wxSize(1100, 620), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) {
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
  exportPortableBtn = new wxButton(this, wxID_ANY, "Export portable bundle...");
  resetBtn = new wxButton(this, wxID_ANY, "Reset to default");
  okBtn = new wxButton(this, wxID_OK, "OK");
  cancelBtn = new wxButton(this, wxID_CANCEL, "Cancel");

  btnSizer->Add(addBtn, 0, wxRIGHT, 5);
  btnSizer->Add(deleteBtn, 0, wxRIGHT, 5);
  btnSizer->Add(downloadBtn, 0, wxRIGHT, 10);
  btnSizer->Add(importBtn, 0, wxRIGHT, 10);
  btnSizer->Add(exportBtn, 0, wxRIGHT, 5);
  btnSizer->Add(exportPortableBtn, 0, wxRIGHT, 5);
  btnSizer->Add(resetBtn, 0, wxRIGHT, 10);
  btnSizer->AddStretchSpacer(1);
  btnSizer->Add(okBtn, 0, wxRIGHT, 5);
  btnSizer->Add(cancelBtn, 0);
  topSizer->Add(btnSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

  SetSizer(topSizer);
  SetMinSize(wxSize(1040, 520));

  addBtn->Bind(wxEVT_BUTTON, &DictionaryEditDialog::OnAdd, this);
  deleteBtn->Bind(wxEVT_BUTTON, &DictionaryEditDialog::OnDelete, this);
  downloadBtn->Bind(wxEVT_BUTTON, &DictionaryEditDialog::OnDownloadGdtf, this);
  importBtn->Bind(wxEVT_BUTTON, &DictionaryEditDialog::OnImportDictionary, this);
  exportBtn->Bind(wxEVT_BUTTON, &DictionaryEditDialog::OnExportDictionary, this);
  exportPortableBtn->Bind(wxEVT_BUTTON, &DictionaryEditDialog::OnExportPortableBundle, this);
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

  fixtureSnapshotAtLoad = BuildFixtureSnapshotFromUi();
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

  trussSnapshotAtLoad = BuildTrussSnapshotFromUi();
}

std::vector<std::string> DictionaryEditDialog::BuildFixtureSnapshotFromUi() const {
  std::vector<std::string> snapshot;
  if (!fixtureTable)
    return snapshot;

  const int count = fixtureTable->GetItemCount();
  snapshot.reserve(static_cast<size_t>(count));
  for (int i = 0; i < count; ++i) {
    wxVariant nameVar;
    fixtureTable->GetValue(nameVar, i, 0);
    const std::string name = std::string(nameVar.GetString().ToUTF8());
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
    const std::string mode = std::string(modeVar.GetString().ToUTF8());
    snapshot.push_back(name + '\n' + path + '\n' + mode);
  }
  std::sort(snapshot.begin(), snapshot.end());
  return snapshot;
}

std::vector<std::string> DictionaryEditDialog::BuildTrussSnapshotFromUi() const {
  std::vector<std::string> snapshot;
  if (!trussTable)
    return snapshot;

  const int count = trussTable->GetItemCount();
  snapshot.reserve(static_cast<size_t>(count));
  for (int i = 0; i < count; ++i) {
    wxVariant nameVar;
    trussTable->GetValue(nameVar, i, 0);
    const std::string name = std::string(nameVar.GetString().ToUTF8());
    if (name.empty())
      continue;
    if (static_cast<size_t>(i) >= trussPaths.size())
      continue;
    const std::string &path = trussPaths[i];
    if (path.empty())
      continue;
    if (!std::filesystem::exists(path))
      continue;
    snapshot.push_back(name + '\n' + path);
  }
  std::sort(snapshot.begin(), snapshot.end());
  return snapshot;
}

bool DictionaryEditDialog::HasFixtureChanges() const {
  return BuildFixtureSnapshotFromUi() != fixtureSnapshotAtLoad;
}

bool DictionaryEditDialog::HasTrussChanges() const {
  return BuildTrussSnapshotFromUi() != trussSnapshotAtLoad;
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
        "Loaded default dictionary because the user dictionary file had an error.",
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
    auto copied = CopyToLibrary(this, path, "fixtures");
    if (!copied)
      continue;
    rows.push_back({name, copied->path, mode, copied->source, copied->sha256});
  }

  SortFixtureRows(rows);
  std::unordered_map<std::string, GdtfDictionary::Entry> dict;
  dict.reserve(rows.size());
  for (const auto &row : rows) {
    GdtfDictionary::Entry entry;
    entry.path = row.path;
    entry.mode = row.mode;
    entry.source = row.source;
    entry.importedAt = FileImportUtils::NowUtcIso8601();
    if (!row.sha256.empty())
      entry.sha256 = row.sha256;
    else if (const auto hash = FileImportUtils::ComputeFileSha256(std::filesystem::u8path(row.path)))
      entry.sha256 = *hash;
    dict[row.name] = std::move(entry);
  }
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
    auto copied = CopyToLibrary(this, path, "trusses");
    if (!copied)
      continue;
    rows.push_back({name, copied->path});
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
  const bool fixtureChanged = HasFixtureChanges();
  const bool trussChanged = HasTrussChanges();

  if (!fixtureChanged && !trussChanged) {
    EndModal(wxID_OK);
    return;
  }

  std::unique_ptr<wxBusyInfo> saveOverlay =
      std::make_unique<wxBusyInfo>("Saving dictionary changes...");
  if (fixtureChanged)
    SaveFixtures();
  if (trussChanged)
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
                          wxEmptyString,
                          "Dictionary files (*.json;*.zip)|*.json;*.zip|JSON files (*.json)|*.json|ZIP files (*.zip)|*.zip",
                          wxFD_OPEN | wxFD_FILE_MUST_EXIST);
  if (fileDialog.ShowModal() != wxID_OK)
    return false;
  const std::string selectedPath = std::string(fileDialog.GetPath().ToUTF8());
  DictionaryBundle::PreparedImport preparedImport =
      DictionaryBundle::PrepareBundleImport(selectedPath, DictionaryBundle::Type::Fixtures);
  if (!preparedImport.errors.empty()) {
    DictionaryImportSummary errorSummary;
    errorSummary.errors = preparedImport.errors;
    wxMessageBox("Invalid fixtures dictionary file.\n\n" +
                     BuildSummaryText(errorSummary),
                 "Import fixtures dictionary", wxOK | wxICON_ERROR, this);
    DictionaryBundle::CleanupPreparedImport(preparedImport);
    return false;
  }
  const std::string importPath = preparedImport.is_bundle
                                     ? preparedImport.rewritten_snapshot_path.string()
                                     : selectedPath;
  const auto validationPreview =
      GdtfDictionary::PreviewImportFromFile(importPath, DictionaryImportPolicy::AddMissing);
  if (validationPreview.HasErrors()) {
    wxMessageBox("Invalid fixtures dictionary file.\n\n" +
                     BuildSummaryText(validationPreview),
                 "Import fixtures dictionary", wxOK | wxICON_ERROR, this);
    DictionaryBundle::CleanupPreparedImport(preparedImport);
    return false;
  }

  DictionaryImportPolicy policy = DictionaryImportPolicy::AddMissing;
  if (!AskImportPolicy(this, policy)) {
    DictionaryBundle::CleanupPreparedImport(preparedImport);
    return false;
  }
  if (policy == DictionaryImportPolicy::ReplaceAll &&
      !ConfirmReplaceAllOperation(this, "fixtures")) {
    DictionaryBundle::CleanupPreparedImport(preparedImport);
    return false;
  }

  const auto preview = GdtfDictionary::PreviewImportFromFile(importPath, policy);
  const auto pathValidation = ValidateFixtureImportPaths(importPath);
  wxString confirmText = "Policy:\n" + GetPolicyDescription(policy) +
                         "\n\nPreview summary:\n" + BuildSummaryText(preview) +
                         "\n\nApply import?";
  if (wxMessageBox(confirmText, "Confirm fixtures dictionary import",
                   wxYES_NO | wxICON_QUESTION, this) != wxYES) {
    DictionaryBundle::CleanupPreparedImport(preparedImport);
    return false;
  }
  if (!ConfirmImportMissingPaths(this, "Fixtures import warning", pathValidation)) {
    DictionaryBundle::CleanupPreparedImport(preparedImport);
    return false;
  }

  const auto result = GdtfDictionary::ApplyImportFromFile(importPath, policy);
  DictionaryBundle::CleanupPreparedImport(preparedImport);
  LoadFixtures();
  wxMessageBox("Fixtures dictionary import completed.\n\n" +
                   BuildSummaryText(result),
               "Fixtures dictionary import", wxOK | wxICON_INFORMATION, this);
  return !result.HasErrors();
}

bool DictionaryEditDialog::ImportTrussesDictionary() {
  wxFileDialog fileDialog(this, "Import trusses dictionary", wxEmptyString,
                          wxEmptyString,
                          "Dictionary files (*.json;*.zip)|*.json;*.zip|JSON files (*.json)|*.json|ZIP files (*.zip)|*.zip",
                          wxFD_OPEN | wxFD_FILE_MUST_EXIST);
  if (fileDialog.ShowModal() != wxID_OK)
    return false;
  const std::string selectedPath = std::string(fileDialog.GetPath().ToUTF8());
  DictionaryBundle::PreparedImport preparedImport =
      DictionaryBundle::PrepareBundleImport(selectedPath, DictionaryBundle::Type::Trusses);
  if (!preparedImport.errors.empty()) {
    DictionaryImportSummary errorSummary;
    errorSummary.errors = preparedImport.errors;
    wxMessageBox("Invalid trusses dictionary file.\n\n" +
                     BuildSummaryText(errorSummary),
                 "Import trusses dictionary", wxOK | wxICON_ERROR, this);
    DictionaryBundle::CleanupPreparedImport(preparedImport);
    return false;
  }
  const std::string importPath = preparedImport.is_bundle
                                     ? preparedImport.rewritten_snapshot_path.string()
                                     : selectedPath;
  const auto validationPreview =
      TrussDictionary::PreviewImportFromFile(importPath, DictionaryImportPolicy::AddMissing);
  if (validationPreview.HasErrors()) {
    wxMessageBox("Invalid trusses dictionary file.\n\n" +
                     BuildSummaryText(validationPreview),
                 "Import trusses dictionary", wxOK | wxICON_ERROR, this);
    DictionaryBundle::CleanupPreparedImport(preparedImport);
    return false;
  }

  DictionaryImportPolicy policy = DictionaryImportPolicy::AddMissing;
  if (!AskImportPolicy(this, policy)) {
    DictionaryBundle::CleanupPreparedImport(preparedImport);
    return false;
  }
  if (policy == DictionaryImportPolicy::ReplaceAll &&
      !ConfirmReplaceAllOperation(this, "trusses")) {
    DictionaryBundle::CleanupPreparedImport(preparedImport);
    return false;
  }

  const auto preview = TrussDictionary::PreviewImportFromFile(importPath, policy);
  const auto pathValidation = ValidateTrussImportPaths(importPath);
  wxString confirmText = "Policy:\n" + GetPolicyDescription(policy) +
                         "\n\nPreview summary:\n" + BuildSummaryText(preview) +
                         "\n\nApply import?";
  if (wxMessageBox(confirmText, "Confirm trusses dictionary import",
                   wxYES_NO | wxICON_QUESTION, this) != wxYES) {
    DictionaryBundle::CleanupPreparedImport(preparedImport);
    return false;
  }
  if (!ConfirmImportMissingPaths(this, "Trusses import warning", pathValidation)) {
    DictionaryBundle::CleanupPreparedImport(preparedImport);
    return false;
  }

  const auto result = TrussDictionary::ApplyImportFromFile(importPath, policy);
  DictionaryBundle::CleanupPreparedImport(preparedImport);
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

void DictionaryEditDialog::OnExportPortableBundle(wxCommandEvent &WXUNUSED(event)) {
  if (IsFixturesPage()) {
    (void)ExportFixturesPortableBundle();
    return;
  }
  (void)ExportTrussesPortableBundle();
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

  const auto exportSummary = AnalyzeFixtureExportPaths(*dictOpt);
  if (!ConfirmExportReferences(this, "Export fixtures dictionary", exportSummary))
    return false;

  wxFileDialog fileDialog(this, "Export fixtures dictionary", wxEmptyString,
                          "gdtf_dictionary_snapshot.json",
                          "JSON files (*.json)|*.json",
                          wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if (fileDialog.ShowModal() != wxID_OK)
    return false;

  bool copyReferencedAssets = false;
  if (!AskCopyReferencedAssets(this, "Export fixtures dictionary options",
                               copyReferencedAssets)) {
    return false;
  }

  const std::string outputPath = std::string(fileDialog.GetPath().ToUTF8());
  SnapshotExportResult exportResult;
  if (!SaveFixturesSnapshotToFile(outputPath, *dictOpt, copyReferencedAssets,
                                  exportResult)) {
    wxMessageBox("Could not write fixtures dictionary snapshot.",
                 "Export fixtures dictionary", wxICON_ERROR | wxOK, this);
    return false;
  }

  wxString info = "Fixtures dictionary snapshot exported successfully.";
  if (copyReferencedAssets) {
    info += "\nCopied assets: " + wxString::Format("%zu", exportResult.copied_assets);
    if (exportResult.missing_assets > 0) {
      info += "\nMissing assets: " +
              wxString::Format("%zu", exportResult.missing_assets);
      const size_t exampleCount = std::min<size_t>(exportResult.copy_errors.size(), 5);
      for (size_t i = 0; i < exampleCount; ++i)
        info += "\n- " + wxString::FromUTF8(exportResult.copy_errors[i]);
    }
  }
  wxMessageBox(info, "Export fixtures dictionary", wxICON_INFORMATION | wxOK,
               this);
  return true;
}

bool DictionaryEditDialog::ExportTrussesDictionary() {
  auto dictOpt = TrussDictionary::Load();
  if (!dictOpt) {
    wxMessageBox("Could not load trusses dictionary for export.",
                 "Export trusses dictionary", wxICON_ERROR | wxOK, this);
    return false;
  }

  const auto exportSummary = AnalyzeTrussExportPaths(*dictOpt);
  if (!ConfirmExportReferences(this, "Export trusses dictionary", exportSummary))
    return false;

  wxFileDialog fileDialog(this, "Export trusses dictionary", wxEmptyString,
                          "truss_dictionary_snapshot.json",
                          "JSON files (*.json)|*.json",
                          wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if (fileDialog.ShowModal() != wxID_OK)
    return false;

  bool copyReferencedAssets = false;
  if (!AskCopyReferencedAssets(this, "Export trusses dictionary options",
                               copyReferencedAssets)) {
    return false;
  }

  const std::string outputPath = std::string(fileDialog.GetPath().ToUTF8());
  SnapshotExportResult exportResult;
  if (!SaveTrussesSnapshotToFile(outputPath, *dictOpt, copyReferencedAssets,
                                 exportResult)) {
    wxMessageBox("Could not write trusses dictionary snapshot.",
                 "Export trusses dictionary", wxICON_ERROR | wxOK, this);
    return false;
  }

  wxString info = "Trusses dictionary snapshot exported successfully.";
  if (copyReferencedAssets) {
    info += "\nCopied assets: " + wxString::Format("%zu", exportResult.copied_assets);
    if (exportResult.missing_assets > 0) {
      info += "\nMissing assets: " +
              wxString::Format("%zu", exportResult.missing_assets);
      const size_t exampleCount = std::min<size_t>(exportResult.copy_errors.size(), 5);
      for (size_t i = 0; i < exampleCount; ++i)
        info += "\n- " + wxString::FromUTF8(exportResult.copy_errors[i]);
    }
  }
  wxMessageBox(info, "Export trusses dictionary", wxICON_INFORMATION | wxOK,
               this);
  return true;
}

bool DictionaryEditDialog::ExportFixturesPortableBundle() {
  auto dictOpt = GdtfDictionary::Load();
  if (!dictOpt) {
    wxMessageBox("Could not load fixtures dictionary for portable export.",
                 "Export portable fixtures bundle", wxICON_ERROR | wxOK, this);
    return false;
  }

  wxFileDialog fileDialog(this, "Export portable fixtures bundle", wxEmptyString,
                          "gdtf_dictionary_bundle.zip",
                          "ZIP files (*.zip)|*.zip",
                          wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if (fileDialog.ShowModal() != wxID_OK)
    return false;

  std::string error;
  const std::string outputPath = std::string(fileDialog.GetPath().ToUTF8());
  if (!DictionaryBundle::ExportFixturesBundle(*dictOpt, outputPath, error)) {
    wxMessageBox("Could not write fixtures portable bundle.\n\n" +
                     wxString::FromUTF8(error),
                 "Export portable fixtures bundle", wxICON_ERROR | wxOK, this);
    return false;
  }

  wxMessageBox("Fixtures portable bundle exported successfully.",
               "Export portable fixtures bundle", wxICON_INFORMATION | wxOK, this);
  return true;
}

bool DictionaryEditDialog::ExportTrussesPortableBundle() {
  auto dictOpt = TrussDictionary::Load();
  if (!dictOpt) {
    wxMessageBox("Could not load trusses dictionary for portable export.",
                 "Export portable trusses bundle", wxICON_ERROR | wxOK, this);
    return false;
  }

  wxFileDialog fileDialog(this, "Export portable trusses bundle", wxEmptyString,
                          "truss_dictionary_bundle.zip",
                          "ZIP files (*.zip)|*.zip",
                          wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if (fileDialog.ShowModal() != wxID_OK)
    return false;

  std::string error;
  const std::string outputPath = std::string(fileDialog.GetPath().ToUTF8());
  if (!DictionaryBundle::ExportTrussesBundle(*dictOpt, outputPath, error)) {
    wxMessageBox("Could not write trusses portable bundle.\n\n" +
                     wxString::FromUTF8(error),
                 "Export portable trusses bundle", wxICON_ERROR | wxOK, this);
    return false;
  }

  wxMessageBox("Trusses portable bundle exported successfully.",
               "Export portable trusses bundle", wxICON_INFORMATION | wxOK, this);
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
