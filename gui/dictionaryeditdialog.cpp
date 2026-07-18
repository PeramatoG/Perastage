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
#include "active_dictionary_storage.h"
#include "dictionary_duplicate.h"
#include "dictionary_editor_state.h"
#include "filesystem_path_utils.h"

#include "colorfulrenderers.h"
#include "colorstore.h"
#include "columnutils.h"
#include "dictionary_bundle.h"
#include "dictionary_export_conflict_dialog.h"
#include "dictionary_json_contract.h"
#include "dictionary_reset_service.h"
#include "dictionary_selection_controls.h"
#include "file_import_utils.h"
#include "gdtf_fixture_category.h"
#include "gdtfdictionary.h"
#include "gdtfloader.h"
#include "json.hpp"
#include "mainwindow.h"
#include "projectutils.h"
#include "table_column_indices.h"
#include "trussdictionary.h"
#include "truss_asset_ingestion.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <vector>

#include <wx/colordlg.h>
#include <wx/dcmemory.h>
#include <wx/filename.h>

namespace {
using FixtureDictionaryColumn = DictionaryFixtureTableColumns::Column;
using TrussDictionaryColumn = DictionaryTrussTableColumns::Column;

constexpr int kFixtureNameColumn =
    TableColumnIndices::ToIndex(FixtureDictionaryColumn::Name);
constexpr int kFixtureFileColumn =
    TableColumnIndices::ToIndex(FixtureDictionaryColumn::File);
constexpr int kFixtureModeColumn =
    TableColumnIndices::ToIndex(FixtureDictionaryColumn::Mode);
constexpr int kFixtureCategoryColumn =
    TableColumnIndices::ToIndex(FixtureDictionaryColumn::Category);
constexpr int kFixtureVisualColorColumn =
    TableColumnIndices::ToIndex(FixtureDictionaryColumn::VisualColor);
constexpr int kTrussNameColumn =
    TableColumnIndices::ToIndex(TrussDictionaryColumn::Name);
constexpr int kTrussFileColumn =
    TableColumnIndices::ToIndex(TrussDictionaryColumn::File);

std::optional<std::string> NormalizeFixtureHexColor(const std::string &raw) {
  std::string value;
  value.reserve(raw.size());
  for (char ch : raw) {
    if (!std::isspace(static_cast<unsigned char>(ch)))
      value.push_back(ch);
  }
  if (value.empty())
    return std::string();
  if (value.front() == '#')
    value.erase(value.begin());
  if (value.size() != 6)
    return std::nullopt;
  for (char &ch : value) {
    if (!std::isxdigit(static_cast<unsigned char>(ch)))
      return std::nullopt;
    ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
  }
  return "#" + value;
}

std::string ExtractFixtureVisualColorText(const wxVariant &value) {
  if (value.GetType() == "wxDataViewIconText") {
    wxDataViewIconText iconText;
    iconText << value;
    return std::string(iconText.GetText().ToUTF8());
  }
  return std::string(value.GetString().ToUTF8());
}

std::string NormalizeFixtureModeKey(std::string value) {
  std::transform(
      value.begin(), value.end(), value.begin(),
      [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  value.erase(std::remove_if(value.begin(), value.end(),
                             [](unsigned char ch) {
                               return std::isspace(ch) != 0 || ch == '_' ||
                                      ch == '-';
                             }),
              value.end());
  return value;
}

bool FixturePathsMatchForColorFamily(const std::string &lhs,
                                     const std::string &rhs) {
  if (lhs.empty() || rhs.empty())
    return false;
  const std::filesystem::path leftPath =
      PathUtils::PathFromUtf8(lhs).lexically_normal();
  const std::filesystem::path rightPath =
      PathUtils::PathFromUtf8(rhs).lexically_normal();
  if (leftPath == rightPath)
    return true;
  if (!leftPath.filename().empty() &&
      leftPath.filename() == rightPath.filename())
    return true;
  std::error_code ec;
  return std::filesystem::exists(leftPath, ec) && !ec &&
         std::filesystem::exists(rightPath, ec) && !ec &&
         std::filesystem::equivalent(leftPath, rightPath, ec) && !ec;
}

void SetFixtureVisualColorCell(wxDataViewListCtrl *table, int row,
                               const std::string &hexColor) {
  if (!table || row == wxNOT_FOUND)
    return;

  wxVariant colorValue;
  const auto normalized = NormalizeFixtureHexColor(hexColor);
  if (!normalized.has_value() || normalized->empty()) {
    colorValue << wxDataViewIconText(wxString(), wxNullBitmap);
    table->SetValue(colorValue, row, kFixtureVisualColorColumn);
    return;
  }

  wxBitmap colorSwatch(16, 16);
  {
    wxMemoryDC dc(colorSwatch);
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxBrush(wxColour(wxString::FromUTF8(*normalized))));
    dc.DrawRectangle(0, 0, 16, 16);
    dc.SelectObject(wxNullBitmap);
  }
  colorValue << wxDataViewIconText(wxString::FromUTF8(*normalized),
                                   colorSwatch);
  table->SetValue(colorValue, row, kFixtureVisualColorColumn);
}

bool IsRedCell(const ColorfulDataViewListStore *store, int row, int col) {
  if (!store || row < 0 || col < 0)
    return false;

  const size_t rowIndex = static_cast<size_t>(row);
  const size_t colIndex = static_cast<size_t>(col);
  if (rowIndex >= store->cellAttrs.size())
    return false;
  if (colIndex >= store->cellAttrs[rowIndex].size())
    return false;

  const wxDataViewItemAttr &attr = store->cellAttrs[rowIndex][colIndex];
  return attr.HasColour() && attr.GetColour() == *wxRED;
}

void SetTableAndChildTooltips(wxDataViewListCtrl *table,
                              const wxString &tooltip) {
  if (!table)
    return;

  table->SetToolTip(tooltip);
  wxWindowList &children = table->GetChildren();
  for (wxWindowList::compatibility_iterator it = children.GetFirst(); it;
       it = it->GetNext()) {
    if (wxWindow *child = it->GetData())
      child->SetToolTip(tooltip);
  }
}

wxPoint NormalizeMousePositionForTable(wxDataViewListCtrl *table,
                                       const wxMouseEvent &event) {
  wxPoint position = event.GetPosition();
  wxWindow *sourceWindow = dynamic_cast<wxWindow *>(event.GetEventObject());
  if (!table || !sourceWindow || sourceWindow == table)
    return position;

  return table->ScreenToClient(sourceWindow->ClientToScreen(position));
}

template <typename Owner>
void BindTableHoverEvents(wxDataViewListCtrl *table, Owner *owner,
                          void (Owner::*onMouseMove)(wxMouseEvent &),
                          void (Owner::*onMouseLeave)(wxMouseEvent &)) {
  if (!table || !owner)
    return;

  auto bindEvents = [&](wxWindow *window) {
    if (!window)
      return;
    window->Unbind(wxEVT_MOTION, onMouseMove, owner);
    window->Unbind(wxEVT_LEAVE_WINDOW, onMouseLeave, owner);
    window->Bind(wxEVT_MOTION, onMouseMove, owner);
    window->Bind(wxEVT_LEAVE_WINDOW, onMouseLeave, owner);
  };

  bindEvents(table);
  wxWindowList &children = table->GetChildren();
  for (wxWindowList::compatibility_iterator it = children.GetFirst(); it;
       it = it->GetNext()) {
    bindEvents(it->GetData());
  }
}

struct FixtureRow {
  std::string name;
  std::string path;
  std::string mode;
  std::string category;
  std::string visualColorHex;
  std::string sha256;
};

struct TrussRow {
  std::string name;
  std::string path;
};

struct CopiedLibraryAsset {
  std::string path;
  std::string sha256;
};

struct ExportPathStatusSummary {
  size_t total_entries = 0;
  size_t found_entries = 0;
  size_t missing_entries = 0;
  std::vector<std::string> missing_files;
};

struct SnapshotExportResult {
  bool success = false;
  size_t copied_assets = 0;
  size_t missing_assets = 0;
  std::vector<std::string> copy_errors;
};

struct SnapshotExportConflictResolution {
  bool accepted = true;
  std::unordered_map<std::string, bool> useNewByFileName;
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

std::filesystem::path
ResolveImportRelativePath(const std::filesystem::path &jsonPath,
                          const std::filesystem::path &rawPath) {
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
  constexpr size_t kMaxMissingExamples = 10;
  summary.total_entries = dict.size();
  for (const auto &[name, entry] : dict) {
    if (HasExistingPath(PathUtils::PathFromUtf8(entry.path)))
      ++summary.found_entries;
    else {
      ++summary.missing_entries;
      if (summary.missing_files.size() < kMaxMissingExamples) {
        const std::string fileName =
            std::filesystem::path(entry.path).filename().string();
        summary.missing_files.push_back(fileName.empty() ? name : fileName);
      }
    }
  }
  return summary;
}

ExportPathStatusSummary AnalyzeTrussExportPaths(
    const std::unordered_map<std::string, std::string> &dict) {
  ExportPathStatusSummary summary;
  constexpr size_t kMaxMissingExamples = 10;
  summary.total_entries = dict.size();
  for (const auto &[name, path] : dict) {
    if (HasExistingPath(PathUtils::PathFromUtf8(path)))
      ++summary.found_entries;
    else {
      ++summary.missing_entries;
      if (summary.missing_files.size() < kMaxMissingExamples) {
        const std::string fileName =
            std::filesystem::path(path).filename().string();
        summary.missing_files.push_back(fileName.empty() ? name : fileName);
      }
    }
  }
  return summary;
}

bool ConfirmExportReferences(wxWindow *parent, const wxString &title,
                             const ExportPathStatusSummary &summary) {
  wxString message = "Found file references in the loaded dictionary.\n\n"
                     "Total entries: " +
                     wxString::Format("%zu", summary.total_entries) + "\n" +
                     "Entries with file found: " +
                     wxString::Format("%zu", summary.found_entries) + "\n" +
                     "Entries with missing file: " +
                     wxString::Format("%zu", summary.missing_entries);
  if (!summary.missing_files.empty()) {
    message += "\nMissing files:";
    for (const auto &file : summary.missing_files)
      message += "\n- " + wxString::FromUTF8(file);
    if (summary.missing_entries > summary.missing_files.size())
      message += "\n- ...";
  }
  message += "\n\nDo you want to export references only?";

  wxMessageDialog confirmDialog(parent, message, title,
                                wxOK | wxCANCEL | wxICON_WARNING);
  confirmDialog.SetOKCancelLabels("Export references only", "Cancel");
  return confirmDialog.ShowModal() == wxID_OK;
}

std::optional<std::string> CopySnapshotAsset(
    const std::filesystem::path &sourcePath,
    const std::filesystem::path &targetAssetsDir,
    const std::unordered_map<std::string, bool> &useNewByFileName) {
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
  const auto keepExistingIt = useNewByFileName.find(fileName);
  if (keepExistingIt != useNewByFileName.end() && !keepExistingIt->second &&
      HasExistingPath(destPath)) {
    return fileName;
  }
  std::filesystem::copy_file(sourcePath, destPath,
                             std::filesystem::copy_options::overwrite_existing,
                             ec);
  if (ec)
    return std::nullopt;
  return fileName;
}

template <typename GatherPathsFn>
SnapshotExportConflictResolution
ResolveExportConflicts(wxWindow *parent, const wxString &title,
                       const std::filesystem::path &outputPath,
                       GatherPathsFn &&gatherSourcePaths) {
  SnapshotExportConflictResolution resolution;
  const std::filesystem::path outputDir = outputPath.parent_path();

  std::vector<std::filesystem::path> sourcePaths = gatherSourcePaths();
  std::sort(sourcePaths.begin(), sourcePaths.end(),
            [](const std::filesystem::path &a, const std::filesystem::path &b) {
              return a.filename().string() < b.filename().string();
            });
  sourcePaths.erase(std::unique(sourcePaths.begin(), sourcePaths.end()),
                    sourcePaths.end());

  std::vector<DictionaryExportConflictDialog::Item> conflicts;
  for (const std::filesystem::path &sourcePath : sourcePaths) {
    if (!HasExistingPath(sourcePath))
      continue;
    const std::string fileName = sourcePath.filename().string();
    if (fileName.empty())
      continue;
    const std::filesystem::path destinationPath = outputDir / fileName;
    if (!HasExistingPath(destinationPath))
      continue;

    const auto sourceHash = FileImportUtils::ComputeFileSha256(sourcePath);
    const auto destinationHash =
        FileImportUtils::ComputeFileSha256(destinationPath);
    if (sourceHash && destinationHash && *sourceHash == *destinationHash)
      continue;

    conflicts.push_back(DictionaryExportConflictDialog::Item{
        fileName, destinationPath.string(), sourcePath.string(), true});
  }

  std::sort(conflicts.begin(), conflicts.end(),
            [](const DictionaryExportConflictDialog::Item &a,
               const DictionaryExportConflictDialog::Item &b) {
              return a.file_name < b.file_name;
            });

  if (conflicts.empty())
    return resolution;

  DictionaryExportConflictDialog dialog(parent, title, std::move(conflicts));
  if (dialog.ShowModal() != wxID_OK) {
    resolution.accepted = false;
    return resolution;
  }
  for (const auto &item : dialog.GetItems())
    resolution.useNewByFileName[item.file_name] = item.use_new;
  return resolution;
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

ImportPathValidationSummary
ValidateFixtureImportPaths(const std::string &importPath) {
  ImportPathValidationSummary summary;
  std::string loadError;
  auto rootOpt = LoadJsonFromFile(importPath, loadError);
  if (!rootOpt)
    return summary;

  std::string parseError;
  auto entriesOpt = DictionaryJsonContract::GetEntriesForType(
      *rootOpt, "fixtures", parseError);
  if (!entriesOpt)
    return summary;

  const std::filesystem::path importFilePath =
      PathUtils::PathFromUtf8(importPath);
  const std::filesystem::path libraryDir =
      PathUtils::PathFromUtf8(ProjectUtils::GetWritableLibraryPath("fixtures"));

  auto checkEntry = [&](const std::string &entryName,
                        const nlohmann::json &entryJson) {
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
    const std::filesystem::path sourcePath = PathUtils::PathFromUtf8(rawPath);
    const std::filesystem::path importRelativePath =
        ResolveImportRelativePath(importFilePath, sourcePath);
    const std::filesystem::path libraryRelativePath =
        libraryDir / sourcePath.filename();
    if (HasExistingPath(importRelativePath) ||
        HasExistingPath(libraryRelativePath)) {
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

ImportPathValidationSummary
ValidateTrussImportPaths(const std::string &importPath) {
  ImportPathValidationSummary summary;
  std::string loadError;
  auto rootOpt = LoadJsonFromFile(importPath, loadError);
  if (!rootOpt)
    return summary;

  std::string parseError;
  auto entriesOpt = DictionaryJsonContract::GetEntriesForType(
      *rootOpt, "trusses", parseError);
  if (!entriesOpt)
    return summary;

  const std::filesystem::path importFilePath =
      PathUtils::PathFromUtf8(importPath);
  const std::filesystem::path libraryDir =
      PathUtils::PathFromUtf8(ProjectUtils::GetWritableLibraryPath("trusses"));

  auto checkEntry = [&](const std::string &entryName,
                        const nlohmann::json &entryJson) {
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
    const std::filesystem::path sourcePath = PathUtils::PathFromUtf8(rawPath);
    const std::filesystem::path importRelativePath =
        ResolveImportRelativePath(importFilePath, sourcePath);
    const std::filesystem::path libraryRelativePath =
        libraryDir / sourcePath.filename();
    if (HasExistingPath(importRelativePath) ||
        HasExistingPath(libraryRelativePath)) {
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
  oss << "Some imported file references could not be resolved before "
         "applying:\n\n";
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
      "Source: " +
          wxString::FromUTF8(sourcePath.string()) +
          "\n"
          "Destination: " +
          wxString::FromUTF8(destPath.string()) + "\n\nChoose conflict policy:",
      "File conflict", choices);

  if (dialog.ShowModal() != wxID_OK)
    return std::nullopt;

  if (dialog.GetSelection() == 0)
    return FileImportUtils::ConflictPolicy::Rename;
  if (dialog.GetSelection() == 1)
    return FileImportUtils::ConflictPolicy::Overwrite;
  return FileImportUtils::ConflictPolicy::Cancel;
}

std::optional<CopiedLibraryAsset> CopyToLibrary(wxWindow *parent,
                                                const std::string &path,
                                                const char *libraryName) {
  if (path.empty())
    return std::nullopt;

  const std::filesystem::path src = PathUtils::PathFromUtf8(path);
  if (!std::filesystem::exists(src))
    return std::nullopt;

  const bool isFixtures = std::string(libraryName) == "fixtures";
  const std::filesystem::path activeDictionaryPath = PathUtils::PathFromUtf8(
      isFixtures ? GdtfDictionary::GetActiveDictionaryFilePath()
                 : TrussDictionary::GetActiveDictionaryFilePath());
  const std::filesystem::path defaultDictionaryPath =
      PathUtils::PathFromUtf8(
          ProjectUtils::GetWritableLibraryPath(libraryName)) /
      (isFixtures ? "gdtf_dictionary.json" : "truss_dictionary.json");
  if (activeDictionaryPath.empty() || defaultDictionaryPath.empty())
    return CopiedLibraryAsset{path, {}};
  const auto layout = ActiveDictionaryStorage::BuildLayout(
      isFixtures ? ActiveDictionaryStorage::DictionaryKind::Fixtures
                 : ActiveDictionaryStorage::DictionaryKind::Trusses,
      activeDictionaryPath, defaultDictionaryPath);

  const std::filesystem::path dest =
      ActiveDictionaryStorage::GetAssetDestination(layout, src);
  FileImportUtils::ConflictPolicy policy =
      FileImportUtils::ConflictPolicy::Overwrite;

  std::error_code ec;
  if (std::filesystem::exists(dest, ec) && !ec) {
    const auto srcHash = FileImportUtils::ComputeFileSha256(src);
    const auto dstHash = FileImportUtils::ComputeFileSha256(dest);
    if (srcHash && dstHash && *srcHash != *dstHash) {
      auto chosenPolicy = AskConflictPolicy(parent, src, dest);
      if (!chosenPolicy ||
          *chosenPolicy == FileImportUtils::ConflictPolicy::Cancel)
        return std::nullopt;
      policy = *chosenPolicy;
    }
  }

  const auto copyResult =
      ActiveDictionaryStorage::CopyAssetIntoDictionaryStorage(
          {isFixtures ? ActiveDictionaryStorage::DictionaryKind::Fixtures
                      : ActiveDictionaryStorage::DictionaryKind::Trusses,
           activeDictionaryPath,
           defaultDictionaryPath,
           src,
           {},
           policy});
  if (!copyResult.success)
    return std::nullopt;

  return CopiedLibraryAsset{copyResult.finalPath.string(),
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
  std::sort(
      rows.begin(), rows.end(),
      [](const FixtureRow &a, const FixtureRow &b) { return a.name < b.name; });
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

bool ConfirmReplaceAllOperation(wxWindow *parent,
                                const wxString &dictionaryName) {
  const wxString message =
      "This will replace all current entries in " + dictionaryName +
      " dictionary.\nThis action is destructive.\n\nContinue?";
  return wxMessageBox(message, "Confirm replace all",
                      wxYES_NO | wxNO_DEFAULT | wxICON_WARNING,
                      parent) == wxYES;
}

bool SaveFixturesSnapshotToFile(
    wxWindow *parent, const std::string &outputPath,
    const std::unordered_map<std::string, GdtfDictionary::Entry> &dict,
    bool copyReferencedAssets, SnapshotExportResult &exportResult) {
  std::vector<std::string> keys;
  keys.reserve(dict.size());
  for (const auto &[name, _] : dict)
    keys.push_back(name);
  std::sort(keys.begin(), keys.end());

  const std::filesystem::path outputFsPath =
      PathUtils::PathFromUtf8(outputPath);
  const std::filesystem::path targetAssetsDir = outputFsPath.parent_path();
  std::unordered_map<std::string, bool> useNewByFileName;
  if (copyReferencedAssets) {
    auto resolution = ResolveExportConflicts(
        parent, "Resolve fixture export conflicts", outputFsPath, [&dict]() {
          std::vector<std::filesystem::path> paths;
          paths.reserve(dict.size());
          for (const auto &[_, entry] : dict) {
            if (!entry.path.empty())
              paths.push_back(PathUtils::PathFromUtf8(entry.path));
          }
          return paths;
        });
    if (!resolution.accepted)
      return false;
    useNewByFileName = std::move(resolution.useNewByFileName);
  }

  nlohmann::json entries = nlohmann::json::object();
  for (const auto &name : keys) {
    const auto &entry = dict.at(name);
    if (entry.path.empty() && entry.mode.empty() && entry.category.empty() &&
        entry.visualColorHex.empty())
      continue;
    nlohmann::json obj;
    if (!entry.path.empty()) {
      const std::filesystem::path sourcePath =
          PathUtils::PathFromUtf8(entry.path);
      if (copyReferencedAssets) {
        const auto copiedRelativePath =
            CopySnapshotAsset(sourcePath, targetAssetsDir, useNewByFileName);
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
    if (!entry.visualColorHex.empty())
      obj["visual_color"] = entry.visualColorHex;
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

// Summarizes the result of a completed dictionary duplication.
wxString BuildDuplicateSummary(const DictionaryDuplicate::Result &result) {
  wxString message = "Dictionary duplicated successfully.";
  message +=
      "\nCopied assets: " + wxString::Format("%zu", result.copiedAssetCount);
  if (result.unresolvedReferenceCount > 0) {
    message += "\nUnresolved references preserved: " +
               wxString::Format("%zu", result.unresolvedReferenceCount);
    const size_t shown =
        std::min<size_t>(result.unresolvedReferences.size(), 5);
    for (size_t i = 0; i < shown; ++i)
      message += "\n- " + wxString::FromUTF8(result.unresolvedReferences[i]);
    if (result.unresolvedReferences.size() > shown)
      message += "\n- ...";
  }
  return message;
}

// Summarizes fatal dictionary duplication errors.
wxString BuildDuplicateErrorSummary(const DictionaryDuplicate::Result &result) {
  wxString message = "Could not duplicate the dictionary.";
  for (const auto &error : result.errors)
    message += "\n- " + wxString::FromUTF8(error);
  return message;
}

bool SaveTrussesSnapshotToFile(
    wxWindow *parent, const std::string &outputPath,
    const std::unordered_map<std::string, std::string> &dict,
    bool copyReferencedAssets, SnapshotExportResult &exportResult) {
  std::vector<std::string> keys;
  keys.reserve(dict.size());
  for (const auto &[name, _] : dict)
    keys.push_back(name);
  std::sort(keys.begin(), keys.end());

  const std::filesystem::path outputFsPath =
      PathUtils::PathFromUtf8(outputPath);
  const std::filesystem::path targetAssetsDir = outputFsPath.parent_path();
  std::unordered_map<std::string, bool> useNewByFileName;
  if (copyReferencedAssets) {
    auto resolution = ResolveExportConflicts(
        parent, "Resolve truss export conflicts", outputFsPath, [&dict]() {
          std::vector<std::filesystem::path> paths;
          paths.reserve(dict.size());
          for (const auto &[_, path] : dict) {
            if (!path.empty())
              paths.push_back(PathUtils::PathFromUtf8(path));
          }
          return paths;
        });
    if (!resolution.accepted)
      return false;
    useNewByFileName = std::move(resolution.useNewByFileName);
  }

  nlohmann::json entries = nlohmann::json::object();
  for (const auto &name : keys) {
    const auto &path = dict.at(name);
    if (path.empty())
      continue;
    const std::filesystem::path sourcePath = PathUtils::PathFromUtf8(path);
    if (copyReferencedAssets) {
      const auto copiedRelativePath =
          CopySnapshotAsset(sourcePath, targetAssetsDir, useNewByFileName);
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
  RefreshDictionarySelectionLabels();
  LoadFixtures();
  LoadTrusses();
  ShowDictionaryLoadStatusMessages();
}

void DictionaryEditDialog::BuildLayout() {
  wxBoxSizer *topSizer = new wxBoxSizer(wxVERTICAL);
  notebook = new wxNotebook(this, wxID_ANY);

  wxPanel *fixturePanel = new wxPanel(notebook);
  wxBoxSizer *fixtureSizer = new wxBoxSizer(wxVERTICAL);
  fixtureStore = new ColorfulDataViewListStore();
  fixtureTable = new wxDataViewListCtrl(
      fixturePanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxDV_ROW_LINES);
  fixtureTable->AssociateModel(fixtureStore);
  fixtureStore->DecRef();
  BindTableHoverEvents(fixtureTable, this,
                       &DictionaryEditDialog::OnFixtureTableMouseMove,
                       &DictionaryEditDialog::OnFixtureTableMouseLeave);
  fixtureTable->CallAfter([this]() {
    BindTableHoverEvents(fixtureTable, this,
                         &DictionaryEditDialog::OnFixtureTableMouseMove,
                         &DictionaryEditDialog::OnFixtureTableMouseLeave);
  });
  int flags = wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE;
  fixtureTable->AppendTextColumn("Name", wxDATAVIEW_CELL_EDITABLE, 200,
                                 wxALIGN_LEFT, flags);
  fixtureTable->AppendTextColumn("File", wxDATAVIEW_CELL_INERT, 260,
                                 wxALIGN_LEFT, flags);
  fixtureTable->AppendTextColumn("Mode", wxDATAVIEW_CELL_INERT, 120,
                                 wxALIGN_LEFT, flags);
  fixtureTable->AppendTextColumn("Category", wxDATAVIEW_CELL_INERT, 130,
                                 wxALIGN_LEFT, flags);
  auto *colorRenderer =
      new ColorfulIconTextRenderer(wxDATAVIEW_CELL_INERT, wxALIGN_LEFT);
  colorRenderer->EnableEllipsize(wxELLIPSIZE_NONE);
  fixtureTable->AppendColumn(new wxDataViewColumn("Type Color", colorRenderer,
                                                  kFixtureVisualColorColumn,
                                                  110, wxALIGN_LEFT, flags));
  ColumnUtils::EnforceMinColumnWidth(fixtureTable);
  fixturesSelection = BuildDictionarySelectionControls(
      fixturePanel, fixtureSizer, "Active fixtures dictionary",
      [this]() {
        wxCommandEvent dummy;
        OnOpenFixturesDictionary(dummy);
      },
      [this]() {
        wxCommandEvent dummy;
        OnNewFixturesDictionary(dummy);
      },
      [this]() {
        wxCommandEvent dummy;
        OnDuplicateFixturesDictionary(dummy);
      },
      [this]() {
        wxCommandEvent dummy;
        OnUseDefaultFixturesDictionary(dummy);
      },
      [this]() {
        wxCommandEvent dummy;
        OnResetDictionary(dummy);
      });
  fixtureSizer->Add(fixtureTable, 1, wxEXPAND | wxALL, 8);
  fixturePanel->SetSizer(fixtureSizer);

  wxPanel *trussPanel = new wxPanel(notebook);
  wxBoxSizer *trussSizer = new wxBoxSizer(wxVERTICAL);
  trussStore = new ColorfulDataViewListStore();
  trussTable = new wxDataViewListCtrl(trussPanel, wxID_ANY, wxDefaultPosition,
                                      wxDefaultSize, wxDV_ROW_LINES);
  trussTable->AssociateModel(trussStore);
  trussStore->DecRef();
  BindTableHoverEvents(trussTable, this,
                       &DictionaryEditDialog::OnTrussTableMouseMove,
                       &DictionaryEditDialog::OnTrussTableMouseLeave);
  trussTable->CallAfter([this]() {
    BindTableHoverEvents(trussTable, this,
                         &DictionaryEditDialog::OnTrussTableMouseMove,
                         &DictionaryEditDialog::OnTrussTableMouseLeave);
  });
  trussTable->AppendTextColumn("Name", wxDATAVIEW_CELL_EDITABLE, 200,
                               wxALIGN_LEFT, flags);
  trussTable->AppendTextColumn("File", wxDATAVIEW_CELL_INERT, 260, wxALIGN_LEFT,
                               flags);
  ColumnUtils::EnforceMinColumnWidth(trussTable);
  trussesSelection = BuildDictionarySelectionControls(
      trussPanel, trussSizer, "Active trusses dictionary",
      [this]() {
        wxCommandEvent dummy;
        OnOpenTrussesDictionary(dummy);
      },
      [this]() {
        wxCommandEvent dummy;
        OnNewTrussesDictionary(dummy);
      },
      [this]() {
        wxCommandEvent dummy;
        OnDuplicateTrussesDictionary(dummy);
      },
      [this]() {
        wxCommandEvent dummy;
        OnUseDefaultTrussesDictionary(dummy);
      },
      [this]() {
        wxCommandEvent dummy;
        OnResetDictionary(dummy);
      });
  trussSizer->Add(trussTable, 1, wxEXPAND | wxALL, 8);
  trussPanel->SetSizer(trussSizer);

  notebook->AddPage(fixturePanel, "Fixtures");
  notebook->AddPage(trussPanel, "Trusses");

  topSizer->Add(notebook, 1, wxEXPAND | wxALL, 8);

  wxBoxSizer *btnSizer = new wxBoxSizer(wxHORIZONTAL);
  addBtn = new wxButton(this, wxID_ADD, "Add");
  deleteBtn = new wxButton(this, wxID_DELETE, "Delete");
  downloadBtn = new wxButton(this, wxID_ANY, "Download Fixture GDTF...");
  importBtn = new wxButton(this, wxID_ANY, "Import...");
  exportBtn = new wxButton(this, wxID_ANY, "Export...");
  okBtn = new wxButton(this, wxID_OK, "OK");
  cancelBtn = new wxButton(this, wxID_CANCEL, "Cancel");

  btnSizer->Add(addBtn, 0, wxRIGHT, 5);
  btnSizer->Add(deleteBtn, 0, wxRIGHT, 5);
  btnSizer->Add(downloadBtn, 0, wxRIGHT, 10);
  btnSizer->Add(importBtn, 0, wxRIGHT, 10);
  btnSizer->Add(exportBtn, 0, wxRIGHT, 10);
  btnSizer->AddStretchSpacer(1);
  btnSizer->Add(okBtn, 0, wxRIGHT, 5);
  btnSizer->Add(cancelBtn, 0);
  topSizer->Add(btnSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

  SetSizer(topSizer);
  SetMinSize(wxSize(1040, 520));

  addBtn->Bind(wxEVT_BUTTON, &DictionaryEditDialog::OnAdd, this);
  deleteBtn->Bind(wxEVT_BUTTON, &DictionaryEditDialog::OnDelete, this);
  downloadBtn->Bind(wxEVT_BUTTON, &DictionaryEditDialog::OnDownloadGdtf, this);
  importBtn->Bind(wxEVT_BUTTON, &DictionaryEditDialog::OnImportDictionary,
                  this);
  exportBtn->Bind(wxEVT_BUTTON, &DictionaryEditDialog::OnExportDictionary,
                  this);
  notebook->Bind(wxEVT_NOTEBOOK_PAGE_CHANGED, [this](wxBookCtrlEvent &event) {
    UpdatePageActionState();
    event.Skip();
  });
  okBtn->Bind(wxEVT_BUTTON, &DictionaryEditDialog::OnOk, this);
  fixtureTable->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED,
                     &DictionaryEditDialog::OnItemActivated, this);
  trussTable->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED,
                   &DictionaryEditDialog::OnItemActivated, this);
  UpdatePageActionState();
}

// Returns whether the fixtures dictionary page is selected.
bool DictionaryEditDialog::IsFixturesPage() const {
  return notebook->GetSelection() == 0;
}

// Updates actions whose availability depends on the selected dictionary page.
void DictionaryEditDialog::UpdatePageActionState() {
  if (!downloadBtn)
    return;

  const bool fixturePage = IsFixturesPage();
  downloadBtn->Show(fixturePage);
  downloadBtn->Enable(fixturePage);
  Layout();
}

void DictionaryEditDialog::OnFixtureTableMouseMove(wxMouseEvent &event) {
  UpdateMissingFileTooltip(fixtureTable, fixtureStore,
                           activeFixtureHoverTooltip,
                           NormalizeMousePositionForTable(fixtureTable, event));
  event.Skip();
}

void DictionaryEditDialog::OnFixtureTableMouseLeave(wxMouseEvent &event) {
  if (!activeFixtureHoverTooltip.IsEmpty()) {
    SetTableAndChildTooltips(fixtureTable, wxString());
    activeFixtureHoverTooltip.clear();
  }
  event.Skip();
}

void DictionaryEditDialog::OnTrussTableMouseMove(wxMouseEvent &event) {
  UpdateMissingFileTooltip(trussTable, trussStore, activeTrussHoverTooltip,
                           NormalizeMousePositionForTable(trussTable, event));
  event.Skip();
}

void DictionaryEditDialog::OnTrussTableMouseLeave(wxMouseEvent &event) {
  if (!activeTrussHoverTooltip.IsEmpty()) {
    SetTableAndChildTooltips(trussTable, wxString());
    activeTrussHoverTooltip.clear();
  }
  event.Skip();
}

void DictionaryEditDialog::UpdateMissingFileTooltip(
    wxDataViewListCtrl *table, ColorfulDataViewListStore *store,
    wxString &activeTooltip, const wxPoint &position) {
  if (!table || !store)
    return;

  wxDataViewItem item;
  wxDataViewColumn *column = nullptr;
  table->HitTest(position, item, column);

  wxString tooltip;
  if (item.IsOk() && column) {
    const int row = table->ItemToRow(item);
    const int modelColumn = column->GetModelColumn();
    if (modelColumn == 1 && IsRedCell(store, row, modelColumn)) {
      tooltip =
          "Referenced file was not found on disk. Update or remove this entry.";
    }
  }

  if (tooltip == activeTooltip)
    return;

  SetTableAndChildTooltips(table, tooltip);
  activeTooltip = tooltip;
}

void DictionaryEditDialog::LoadFixtures() {
  RefreshDictionarySelectionLabels();
  fixtureStore->DeleteAllItems();
  fixturePaths.clear();
  activeFixtureHoverTooltip.clear();
  SetTableAndChildTooltips(fixtureTable, wxString());

  auto dictOpt = GdtfDictionary::Load();
  if (!dictOpt)
    return;

  std::vector<FixtureRow> rows;
  rows.reserve(dictOpt->size());
  for (const auto &[name, entry] : *dictOpt) {
    if (entry.path.empty())
      continue;
    FixtureRow row{name, entry.path, entry.mode, entry.category,
                   entry.visualColorHex};
    rows.push_back(row);
  }
  SortFixtureRows(rows);

  fixturePaths.reserve(rows.size());
  for (const auto &row : rows) {
    const bool fileExists = std::filesystem::exists(row.path);
    wxVector<wxVariant> items;
    items.push_back(wxString::FromUTF8(row.name));
    items.push_back(wxString::FromUTF8(
        std::filesystem::path(row.path).filename().string()));
    items.push_back(wxString::FromUTF8(row.mode));
    items.push_back(wxString::FromUTF8(row.category));
    items.push_back(wxString());
    fixtureStore->AppendItem(items);
    const int rowIndex = fixtureTable->GetItemCount() - 1;
    if (!fileExists && rowIndex >= 0)
      fixtureStore->SetCellTextColour(static_cast<size_t>(rowIndex), 1, *wxRED);
    SetFixtureVisualColorCell(fixtureTable, rowIndex, row.visualColorHex);
    fixturePaths.push_back(row.path);
  }

  fixtureSnapshotAtLoad = BuildFixtureSnapshotFromUi();
}

void DictionaryEditDialog::LoadTrusses() {
  RefreshDictionarySelectionLabels();
  trussStore->DeleteAllItems();
  trussPaths.clear();
  activeTrussHoverTooltip.clear();
  SetTableAndChildTooltips(trussTable, wxString());

  auto dictOpt = TrussDictionary::Load();
  if (!dictOpt)
    return;

  std::vector<TrussRow> rows;
  rows.reserve(dictOpt->size());
  for (const auto &[name, path] : *dictOpt) {
    if (path.empty())
      continue;
    rows.push_back({name, path});
  }
  SortTrussRows(rows);

  trussPaths.reserve(rows.size());
  for (const auto &row : rows) {
    const bool fileExists = std::filesystem::exists(row.path);
    wxVector<wxVariant> items;
    items.push_back(wxString::FromUTF8(row.name));
    items.push_back(wxString::FromUTF8(
        std::filesystem::path(row.path).filename().string()));
    trussStore->AppendItem(items);
    const int rowIndex = trussTable->GetItemCount() - 1;
    if (!fileExists && rowIndex >= 0)
      trussStore->SetCellTextColour(static_cast<size_t>(rowIndex), 1, *wxRED);
    trussPaths.push_back(row.path);
  }

  trussSnapshotAtLoad = BuildTrussSnapshotFromUi();
}

// Builds a dirty-state snapshot for every editable fixture row.
std::vector<std::string>
DictionaryEditDialog::BuildFixtureSnapshotFromUi() const {
  std::vector<DictionaryEditorState::FixtureSnapshotRow> rows;
  if (!fixtureTable)
    return {};

  const int count = fixtureTable->GetItemCount();
  rows.reserve(static_cast<size_t>(count));
  for (int i = 0; i < count; ++i) {
    if (static_cast<size_t>(i) >= fixturePaths.size())
      continue;
    wxVariant nameVar;
    fixtureTable->GetValue(nameVar, i, kFixtureNameColumn);
    wxVariant modeVar;
    fixtureTable->GetValue(modeVar, i, kFixtureModeColumn);
    wxVariant categoryVar;
    fixtureTable->GetValue(categoryVar, i, kFixtureCategoryColumn);
    wxVariant colorVar;
    fixtureTable->GetValue(colorVar, i, kFixtureVisualColorColumn);
    rows.push_back({std::string(nameVar.GetString().ToUTF8()), fixturePaths[i],
                    std::string(modeVar.GetString().ToUTF8()),
                    std::string(categoryVar.GetString().ToUTF8()),
                    ExtractFixtureVisualColorText(colorVar)});
  }
  return DictionaryEditorState::BuildFixtureSnapshot(std::move(rows));
}

// Builds a dirty-state snapshot for every editable truss row.
std::vector<std::string>
DictionaryEditDialog::BuildTrussSnapshotFromUi() const {
  std::vector<DictionaryEditorState::TrussSnapshotRow> rows;
  if (!trussTable)
    return {};

  const int count = trussTable->GetItemCount();
  rows.reserve(static_cast<size_t>(count));
  for (int i = 0; i < count; ++i) {
    if (static_cast<size_t>(i) >= trussPaths.size())
      continue;
    wxVariant nameVar;
    trussTable->GetValue(nameVar, i, kTrussNameColumn);
    rows.push_back({std::string(nameVar.GetString().ToUTF8()), trussPaths[i]});
  }
  return DictionaryEditorState::BuildTrussSnapshot(std::move(rows));
}

bool DictionaryEditDialog::HasFixtureChanges() const {
  return BuildFixtureSnapshotFromUi() != fixtureSnapshotAtLoad;
}

bool DictionaryEditDialog::HasTrussChanges() const {
  return BuildTrussSnapshotFromUi() != trussSnapshotAtLoad;
}

// Prompts for pending edits on one dictionary page before continuing.
bool DictionaryEditDialog::ConfirmDirtyChangesBeforeReload(
    DictionaryEditorState::DictionaryEditorPage page,
    const wxString &operationLabel) {
  const bool fixtureChanged = HasFixtureChanges();
  const bool trussChanged = HasTrussChanges();
  const bool hasChanges =
      DictionaryEditorState::HasPageChanges(page, fixtureChanged, trussChanged);
  if (!hasChanges)
    return true;

  const wxString pageLabel =
      page == DictionaryEditorState::DictionaryEditorPage::Fixtures ? "fixtures"
                                                                   : "trusses";
  wxMessageDialog dialog(
      this,
      "The " + pageLabel +
          " dictionary page has unsaved table edits. Save them before " +
          operationLabel + "?",
      "Unsaved dictionary edits",
      wxYES_NO | wxCANCEL | wxCANCEL_DEFAULT | wxICON_WARNING);
  dialog.SetYesNoCancelLabels("Save", "Discard", "Cancel");
  const int response = dialog.ShowModal();
  DictionaryEditorState::DirtyGuardChoice choice =
      DictionaryEditorState::DirtyGuardChoice::Cancel;
  if (response == wxID_YES)
    choice = DictionaryEditorState::DirtyGuardChoice::Save;
  else if (response == wxID_NO)
    choice = DictionaryEditorState::DirtyGuardChoice::Discard;

  bool saveSucceeded = false;
  if (choice == DictionaryEditorState::DirtyGuardChoice::Save) {
    if (!EnsurePageCanWrite(page))
      return false;
    saveSucceeded = page == DictionaryEditorState::DictionaryEditorPage::Fixtures
                        ? SaveFixtures()
                        : SaveTrusses();
  }

  const auto result = DictionaryEditorState::ResolveDirtyGuard(
      hasChanges, choice, saveSucceeded);
  if (result != DictionaryEditorState::DirtyGuardResult::Continue)
    return false;

  if (choice == DictionaryEditorState::DirtyGuardChoice::Discard)
    ReloadPage(page);
  return true;
}

// Reloads one dictionary editor page from its active dictionary.
void DictionaryEditDialog::ReloadPage(
    DictionaryEditorState::DictionaryEditorPage page) {
  if (page == DictionaryEditorState::DictionaryEditorPage::Fixtures)
    LoadFixtures();
  else
    LoadTrusses();
}

// Ensures the active dictionary page is safe to mutate before a write.
bool DictionaryEditDialog::EnsurePageCanWrite(
    DictionaryEditorState::DictionaryEditorPage page) {
  DictionaryEditorState::LoadRecoveryStatus status;
  if (page == DictionaryEditorState::DictionaryEditorPage::Fixtures) {
    const auto loadStatus = GdtfDictionary::GetLastLoadStatus();
    status = {loadStatus.activeDictionaryInvalid,
              loadStatus.activeDictionaryMissing,
              loadStatus.temporaryFallbackUsed};
  } else {
    const auto loadStatus = TrussDictionary::GetLastLoadStatus();
    status = {loadStatus.activeDictionaryInvalid,
              loadStatus.activeDictionaryMissing,
              loadStatus.temporaryFallbackUsed};
  }
  if (!DictionaryEditorState::RequiresWriteRecovery(status))
    return true;
  return RecoverInvalidActiveDictionary(page);
}

// Runs explicit recovery for an invalid or missing active custom dictionary.
bool DictionaryEditDialog::RecoverInvalidActiveDictionary(
    DictionaryEditorState::DictionaryEditorPage page) {
  const bool fixtures =
      page == DictionaryEditorState::DictionaryEditorPage::Fixtures;
  const wxString label = fixtures ? "fixtures" : "trusses";
  const wxString choices[] = {
      "Open Another Dictionary...",
      "Use Default",
      "Recreate Active Custom Dictionary From Application Defaults...",
      "Cancel"};
  wxSingleChoiceDialog dialog(
      this,
      wxString("The active ") + label +
          " dictionary is invalid or missing, so writes are blocked until "
          "you choose an explicit recovery action.",
      wxString("Recover ") + label + " dictionary", WXSIZEOF(choices),
      choices);
  if (dialog.ShowModal() != wxID_OK || dialog.GetSelection() == 3)
    return false;

  std::string error;
  if (dialog.GetSelection() == 0) {
    const std::filesystem::path currentPath = PathUtils::PathFromUtf8(
        fixtures ? GdtfDictionary::GetActiveDictionaryFilePath()
                 : TrussDictionary::GetActiveDictionaryFilePath());
    wxFileDialog fileDialog(this, wxString("Open ") + label + " dictionary",
                            wxString::FromUTF8(
                                currentPath.parent_path().string()),
                            wxString(),
                            "JSON dictionary files (*.json)|*.json",
                            wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (fileDialog.ShowModal() != wxID_OK)
      return false;
    const std::string selected = std::string(fileDialog.GetPath().ToUTF8());
    const bool opened = fixtures
                            ? GdtfDictionary::SetActiveDictionaryFilePath(
                                  selected, &error)
                            : TrussDictionary::SetActiveDictionaryFilePath(
                                  selected, &error);
    if (!opened) {
      wxMessageBox("Could not open the selected dictionary.\n\n" +
                       wxString::FromUTF8(error),
                   "Recover dictionary", wxOK | wxICON_ERROR, this);
      RefreshDictionarySelectionLabels();
      return false;
    }
  } else if (dialog.GetSelection() == 1) {
    const bool setDefault = fixtures
                                ? GdtfDictionary::SetActiveDictionaryFilePath(
                                      {}, &error)
                                : TrussDictionary::SetActiveDictionaryFilePath(
                                      {}, &error);
    if (!setDefault) {
      wxMessageBox("Could not switch to the default dictionary.\n\n" +
                       wxString::FromUTF8(error),
                   "Recover dictionary", wxOK | wxICON_ERROR, this);
      return false;
    }
  } else {
    const std::string activePath =
        fixtures ? GdtfDictionary::GetActiveDictionaryFilePath()
                 : TrussDictionary::GetActiveDictionaryFilePath();
    const bool recreated = fixtures
                               ? GdtfDictionary::CreateDictionaryFileFromDefaults(
                                     activePath, &error)
                               : TrussDictionary::CreateDictionaryFileFromDefaults(
                                     activePath, &error);
    if (!recreated) {
      wxMessageBox("Could not recreate the active dictionary.\n\n" +
                       wxString::FromUTF8(error),
                   "Recover dictionary", wxOK | wxICON_ERROR, this);
      return false;
    }
  }

  ReloadPage(page);
  RefreshDictionarySelectionLabels();
  return true;
}

void DictionaryEditDialog::ShowDictionaryLoadStatusMessages() {
  wxString message;
  bool hasError = false;

  const auto appendStatus =
      [&](const wxString &label, bool invalid, bool missing, bool fallback,
          bool recreated, const std::string &activePath,
          const std::string &fallbackPath, const std::string &error) {
        if (!invalid && !missing && !fallback && !recreated && error.empty())
          return;
        if (!message.empty())
          message += "\n\n";
        message += label + ": ";
        if (recreated) {
          message += "the managed default dictionary was recreated from "
                     "application defaults.";
        } else if (fallback) {
          message += "a temporary application-default fallback was loaded "
                     "without changing the active dictionary.";
        } else if (invalid) {
          message += "the active dictionary is invalid and was left unchanged.";
        } else if (missing) {
          message += "the active dictionary file is missing.";
        } else {
          message += "dictionary loading reported a warning.";
        }
        if (!activePath.empty())
          message += "\nActive: " + wxString::FromUTF8(activePath);
        if (fallback && !fallbackPath.empty())
          message += "\nFallback: " + wxString::FromUTF8(fallbackPath);
        if (!error.empty())
          message += "\nDetails: " + wxString::FromUTF8(error);
        hasError = hasError || invalid || missing;
      };

  const GdtfDictionary::LoadStatus fixturesStatus =
      GdtfDictionary::GetLastLoadStatus();
  appendStatus("Fixtures", fixturesStatus.activeDictionaryInvalid,
               fixturesStatus.activeDictionaryMissing,
               fixturesStatus.temporaryFallbackUsed,
               fixturesStatus.managedDefaultRecreated,
               fixturesStatus.activePath, fixturesStatus.fallbackPath,
               fixturesStatus.error);

  const TrussDictionary::LoadStatus trussStatus =
      TrussDictionary::GetLastLoadStatus();
  appendStatus("Trusses", trussStatus.activeDictionaryInvalid,
               trussStatus.activeDictionaryMissing,
               trussStatus.temporaryFallbackUsed,
               trussStatus.managedDefaultRecreated, trussStatus.activePath,
               trussStatus.fallbackPath, trussStatus.error);

  if (!message.empty()) {
    wxMessageBox(message,
                 hasError ? "Dictionary load error" : "Dictionary warning",
                 wxOK | (hasError ? wxICON_ERROR : wxICON_WARNING), this);
  }
}

void DictionaryEditDialog::RefreshDictionarySelectionLabels() {
  UpdateDictionarySelectionControls(
      fixturesSelection,
      wxString::FromUTF8(GdtfDictionary::GetActiveDictionaryFileName()),
      wxString::FromUTF8(GdtfDictionary::GetActiveDictionaryFilePath()));
  UpdateDictionarySelectionControls(
      trussesSelection,
      wxString::FromUTF8(TrussDictionary::GetActiveDictionaryFileName()),
      wxString::FromUTF8(TrussDictionary::GetActiveDictionaryFilePath()));
}

// Opens an existing fixtures dictionary after validating its contract.
void DictionaryEditDialog::OnOpenFixturesDictionary(
    wxCommandEvent &WXUNUSED(event)) {
  if (!ConfirmDirtyChangesBeforeReload(DictionaryEditorState::DictionaryEditorPage::Fixtures, "opening a fixtures dictionary"))
    return;
  const std::filesystem::path currentPath =
      PathUtils::PathFromUtf8(GdtfDictionary::GetActiveDictionaryFilePath());
  wxFileDialog dialog(this, "Open fixtures dictionary",
                      wxString::FromUTF8(currentPath.parent_path().string()),
                      wxString(), "JSON dictionary files (*.json)|*.json",
                      wxFD_OPEN | wxFD_FILE_MUST_EXIST);
  if (dialog.ShowModal() != wxID_OK)
    return;
  std::string error;
  const std::string selected = std::string(dialog.GetPath().ToUTF8());
  if (!GdtfDictionary::SetActiveDictionaryFilePath(selected, &error)) {
    wxMessageBox("Could not open fixtures dictionary.\n\n" +
                     wxString::FromUTF8(error),
                 "Open dictionary", wxOK | wxICON_ERROR, this);
    RefreshDictionarySelectionLabels();
    return;
  }
  LoadFixtures();
}

// Opens an existing trusses dictionary after validating its contract.
void DictionaryEditDialog::OnOpenTrussesDictionary(
    wxCommandEvent &WXUNUSED(event)) {
  if (!ConfirmDirtyChangesBeforeReload(DictionaryEditorState::DictionaryEditorPage::Trusses, "opening a trusses dictionary"))
    return;
  const std::filesystem::path currentPath =
      PathUtils::PathFromUtf8(TrussDictionary::GetActiveDictionaryFilePath());
  wxFileDialog dialog(this, "Open trusses dictionary",
                      wxString::FromUTF8(currentPath.parent_path().string()),
                      wxString(), "JSON dictionary files (*.json)|*.json",
                      wxFD_OPEN | wxFD_FILE_MUST_EXIST);
  if (dialog.ShowModal() != wxID_OK)
    return;
  std::string error;
  const std::string selected = std::string(dialog.GetPath().ToUTF8());
  if (!TrussDictionary::SetActiveDictionaryFilePath(selected, &error)) {
    wxMessageBox("Could not open trusses dictionary.\n\n" +
                     wxString::FromUTF8(error),
                 "Open dictionary", wxOK | wxICON_ERROR, this);
    RefreshDictionarySelectionLabels();
    return;
  }
  LoadTrusses();
}

// Creates and activates a new fixtures dictionary.
void DictionaryEditDialog::OnNewFixturesDictionary(
    wxCommandEvent &WXUNUSED(event)) {
  if (!ConfirmDirtyChangesBeforeReload(DictionaryEditorState::DictionaryEditorPage::Fixtures, "creating a fixtures dictionary"))
    return;
  wxArrayString choices;
  choices.Add("Empty dictionary");
  choices.Add("From application defaults (seed once)");
  wxSingleChoiceDialog choiceDialog(
      this, "Choose how to create the new fixtures dictionary.",
      "New fixtures dictionary", choices);
  if (choiceDialog.ShowModal() != wxID_OK)
    return;
  wxFileDialog dialog(this, "Create fixtures dictionary", wxString(),
                      "gdtf_dictionary.json",
                      "JSON dictionary files (*.json)|*.json",
                      wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if (dialog.ShowModal() != wxID_OK)
    return;
  std::filesystem::path selectedPath =
      PathUtils::PathFromUtf8(std::string(dialog.GetPath().ToUTF8()));
  if (!selectedPath.has_extension())
    selectedPath += ".json";
  std::string error;
  const bool created = choiceDialog.GetSelection() == 0
                           ? GdtfDictionary::CreateEmptyDictionaryFile(
                                 selectedPath.string(), &error)
                           : GdtfDictionary::CreateDictionaryFileFromDefaults(
                                 selectedPath.string(), &error);
  if (!created || !GdtfDictionary::SetActiveDictionaryFilePath(
                      selectedPath.string(), &error)) {
    wxMessageBox("Could not create fixtures dictionary.\n\n" +
                     wxString::FromUTF8(error),
                 "New dictionary", wxOK | wxICON_ERROR, this);
    RefreshDictionarySelectionLabels();
    return;
  }
  LoadFixtures();
}

// Creates and activates a new trusses dictionary.
void DictionaryEditDialog::OnNewTrussesDictionary(
    wxCommandEvent &WXUNUSED(event)) {
  if (!ConfirmDirtyChangesBeforeReload(DictionaryEditorState::DictionaryEditorPage::Trusses, "creating a trusses dictionary"))
    return;
  wxArrayString choices;
  choices.Add("Empty dictionary");
  choices.Add("From application defaults (seed once)");
  wxSingleChoiceDialog choiceDialog(
      this, "Choose how to create the new trusses dictionary.",
      "New trusses dictionary", choices);
  if (choiceDialog.ShowModal() != wxID_OK)
    return;
  wxFileDialog dialog(this, "Create trusses dictionary", wxString(),
                      "truss_dictionary.json",
                      "JSON dictionary files (*.json)|*.json",
                      wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if (dialog.ShowModal() != wxID_OK)
    return;
  std::filesystem::path selectedPath =
      PathUtils::PathFromUtf8(std::string(dialog.GetPath().ToUTF8()));
  if (!selectedPath.has_extension())
    selectedPath += ".json";
  std::string error;
  const bool created = choiceDialog.GetSelection() == 0
                           ? TrussDictionary::CreateEmptyDictionaryFile(
                                 selectedPath.string(), &error)
                           : TrussDictionary::CreateDictionaryFileFromDefaults(
                                 selectedPath.string(), &error);
  if (!created || !TrussDictionary::SetActiveDictionaryFilePath(
                      selectedPath.string(), &error)) {
    wxMessageBox("Could not create trusses dictionary.\n\n" +
                     wxString::FromUTF8(error),
                 "New dictionary", wxOK | wxICON_ERROR, this);
    RefreshDictionarySelectionLabels();
    return;
  }
  LoadTrusses();
}

// Duplicates the active fixtures dictionary and optionally activates it.
void DictionaryEditDialog::OnDuplicateFixturesDictionary(
    wxCommandEvent &WXUNUSED(event)) {
  if (!ConfirmDirtyChangesBeforeReload(DictionaryEditorState::DictionaryEditorPage::Fixtures, "duplicating the fixtures dictionary"))
    return;
  const std::filesystem::path currentPath =
      PathUtils::PathFromUtf8(GdtfDictionary::GetActiveDictionaryFilePath());
  const std::filesystem::path defaultPath =
      PathUtils::PathFromUtf8(
          ProjectUtils::GetWritableLibraryPath("fixtures")) /
      "gdtf_dictionary.json";
  wxFileDialog dialog(
      this, "Duplicate fixtures dictionary",
      wxString::FromUTF8(currentPath.parent_path().string()),
      wxString::FromUTF8(
          DictionaryDuplicate::BuildDefaultDuplicatePath(currentPath)
              .filename()
              .string()),
      "JSON dictionary files (*.json)|*.json",
      wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if (dialog.ShowModal() != wxID_OK)
    return;
  std::filesystem::path selectedPath =
      PathUtils::PathFromUtf8(std::string(dialog.GetPath().ToUTF8()));
  if (selectedPath.extension() != ".json")
    selectedPath.replace_extension(".json");

  const auto result = DictionaryDuplicate::DuplicateDictionary(
      {ActiveDictionaryStorage::DictionaryKind::Fixtures, currentPath,
       defaultPath, selectedPath});
  if (!result.success) {
    wxMessageBox(BuildDuplicateErrorSummary(result),
                 "Duplicate fixtures dictionary", wxOK | wxICON_ERROR, this);
    RefreshDictionarySelectionLabels();
    return;
  }

  wxMessageDialog activateDialog(
      this,
      BuildDuplicateSummary(result) +
          "\n\nDo you want to activate the duplicate now?",
      "Duplicate fixtures dictionary", wxYES_NO | wxICON_QUESTION);
  activateDialog.SetYesNoLabels("Activate duplicate", "Keep current");
  if (activateDialog.ShowModal() == wxID_YES) {
    std::string error;
    if (!GdtfDictionary::SetActiveDictionaryFilePath(selectedPath.string(),
                                                     &error)) {
      wxMessageBox("The duplicate was created but could not be activated.\n\n" +
                       wxString::FromUTF8(error),
                   "Duplicate fixtures dictionary", wxOK | wxICON_ERROR, this);
      RefreshDictionarySelectionLabels();
      return;
    }
    LoadFixtures();
    return;
  }
  RefreshDictionarySelectionLabels();
}

// Duplicates the active trusses dictionary and optionally activates it.
void DictionaryEditDialog::OnDuplicateTrussesDictionary(
    wxCommandEvent &WXUNUSED(event)) {
  if (!ConfirmDirtyChangesBeforeReload(DictionaryEditorState::DictionaryEditorPage::Trusses, "duplicating the trusses dictionary"))
    return;
  const std::filesystem::path currentPath =
      PathUtils::PathFromUtf8(TrussDictionary::GetActiveDictionaryFilePath());
  const std::filesystem::path defaultPath =
      PathUtils::PathFromUtf8(ProjectUtils::GetWritableLibraryPath("trusses")) /
      "truss_dictionary.json";
  wxFileDialog dialog(
      this, "Duplicate trusses dictionary",
      wxString::FromUTF8(currentPath.parent_path().string()),
      wxString::FromUTF8(
          DictionaryDuplicate::BuildDefaultDuplicatePath(currentPath)
              .filename()
              .string()),
      "JSON dictionary files (*.json)|*.json",
      wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if (dialog.ShowModal() != wxID_OK)
    return;
  std::filesystem::path selectedPath =
      PathUtils::PathFromUtf8(std::string(dialog.GetPath().ToUTF8()));
  if (selectedPath.extension() != ".json")
    selectedPath.replace_extension(".json");

  const auto result = DictionaryDuplicate::DuplicateDictionary(
      {ActiveDictionaryStorage::DictionaryKind::Trusses, currentPath,
       defaultPath, selectedPath});
  if (!result.success) {
    wxMessageBox(BuildDuplicateErrorSummary(result),
                 "Duplicate trusses dictionary", wxOK | wxICON_ERROR, this);
    RefreshDictionarySelectionLabels();
    return;
  }

  wxMessageDialog activateDialog(
      this,
      BuildDuplicateSummary(result) +
          "\n\nDo you want to activate the duplicate now?",
      "Duplicate trusses dictionary", wxYES_NO | wxICON_QUESTION);
  activateDialog.SetYesNoLabels("Activate duplicate", "Keep current");
  if (activateDialog.ShowModal() == wxID_YES) {
    std::string error;
    if (!TrussDictionary::SetActiveDictionaryFilePath(selectedPath.string(),
                                                      &error)) {
      wxMessageBox("The duplicate was created but could not be activated.\n\n" +
                       wxString::FromUTF8(error),
                   "Duplicate trusses dictionary", wxOK | wxICON_ERROR, this);
      RefreshDictionarySelectionLabels();
      return;
    }
    LoadTrusses();
    return;
  }
  RefreshDictionarySelectionLabels();
}

// Switches fixtures back to the managed default dictionary path.
void DictionaryEditDialog::OnUseDefaultFixturesDictionary(
    wxCommandEvent &WXUNUSED(event)) {
  if (!ConfirmDirtyChangesBeforeReload(
          DictionaryEditorState::DictionaryEditorPage::Fixtures,
          "using the default fixtures dictionary path"))
    return;
  std::string error;
  if (!GdtfDictionary::SetActiveDictionaryFilePath({}, &error)) {
    wxMessageBox("Could not use the default fixtures dictionary path.\n\n" +
                     wxString::FromUTF8(error),
                 "Use Default", wxOK | wxICON_ERROR, this);
    return;
  }
  LoadFixtures();
}

// Switches trusses back to the managed default dictionary path.
void DictionaryEditDialog::OnUseDefaultTrussesDictionary(
    wxCommandEvent &WXUNUSED(event)) {
  if (!ConfirmDirtyChangesBeforeReload(
          DictionaryEditorState::DictionaryEditorPage::Trusses,
          "using the default trusses dictionary path"))
    return;
  std::string error;
  if (!TrussDictionary::SetActiveDictionaryFilePath({}, &error)) {
    wxMessageBox("Could not use the default trusses dictionary path.\n\n" +
                     wxString::FromUTF8(error),
                 "Use Default", wxOK | wxICON_ERROR, this);
    return;
  }
  LoadTrusses();
}

// Persists fixture table rows without dropping unresolved entries.
bool DictionaryEditDialog::SaveFixtures() {
  if (!EnsurePageCanWrite(DictionaryEditorState::DictionaryEditorPage::Fixtures))
    return false;

  std::vector<FixtureRow> rows;
  int count = fixtureTable->GetItemCount();
  rows.reserve(static_cast<size_t>(count));
  for (int i = 0; i < count; ++i) {
    wxVariant nameVar;
    fixtureTable->GetValue(nameVar, i, kFixtureNameColumn);
    std::string name = std::string(nameVar.GetString().ToUTF8());
    if (name.empty())
      continue;
    if (static_cast<size_t>(i) >= fixturePaths.size())
      continue;
    const std::string &path = fixturePaths[i];
    if (path.empty())
      continue;
    wxVariant modeVar;
    fixtureTable->GetValue(modeVar, i, kFixtureModeColumn);
    std::string mode = std::string(modeVar.GetString().ToUTF8());
    CopiedLibraryAsset asset{path, {}};
    if (std::filesystem::exists(path)) {
      auto copied = CopyToLibrary(this, path, "fixtures");
      if (!copied) {
        wxMessageBox("Could not copy fixture file into the library. The "
                     "dictionary was not saved.",
                     "Save fixtures dictionary", wxICON_ERROR | wxOK, this);
        return false;
      }
      asset = *copied;
    }
    wxVariant categoryVar;
    fixtureTable->GetValue(categoryVar, i, kFixtureCategoryColumn);
    const std::string category = GdtfFixtureCategory::NormalizeCategory(
        std::string(categoryVar.GetString().ToUTF8()));
    wxVariant colorVar;
    fixtureTable->GetValue(colorVar, i, kFixtureVisualColorColumn);
    const std::string rawColor = ExtractFixtureVisualColorText(colorVar);
    const auto normalizedColor = NormalizeFixtureHexColor(rawColor);
    if (!normalizedColor.has_value()) {
      wxMessageBox(
          wxString::Format("Invalid color in row %d: '%s'\nExpected #RRGGBB.",
                           i + 1, wxString::FromUTF8(rawColor).c_str()),
          "Save fixtures dictionary", wxICON_ERROR | wxOK, this);
      return false;
    }
    rows.push_back(
        {name, asset.path, mode, category, *normalizedColor, asset.sha256});
  }

  SortFixtureRows(rows);
  std::unordered_map<std::string, GdtfDictionary::Entry> dict;
  dict.reserve(rows.size());
  for (const auto &row : rows) {
    GdtfDictionary::Entry entry;
    entry.path = row.path;
    entry.mode = row.mode;
    entry.category = row.category;
    entry.visualColorHex = row.visualColorHex;
    entry.importedAt = FileImportUtils::NowUtcIso8601();
    if (!row.sha256.empty())
      entry.sha256 = row.sha256;
    else if (const auto hash = FileImportUtils::ComputeFileSha256(
                 PathUtils::PathFromUtf8(row.path)))
      entry.sha256 = *hash;
    dict[row.name] = std::move(entry);
  }
  std::string saveError;
  if (!GdtfDictionary::Save(dict, &saveError)) {
    wxMessageBox("Could not save fixtures dictionary.\n\n" +
                     wxString::FromUTF8(saveError),
                 "Save fixtures dictionary", wxICON_ERROR | wxOK, this);
    return false;
  }

  LoadFixtures();
  return true;
}

// Persists truss table rows without dropping unresolved entries.
bool DictionaryEditDialog::SaveTrusses() {
  if (!EnsurePageCanWrite(DictionaryEditorState::DictionaryEditorPage::Trusses))
    return false;

  std::vector<TrussRow> rows;
  int count = trussTable->GetItemCount();
  rows.reserve(static_cast<size_t>(count));
  std::vector<std::filesystem::path> ingestedPaths;
  auto cleanupIngestedPaths = [&ingestedPaths]() {
    for (const auto &ingestedPath : ingestedPaths) {
      std::error_code ec;
      std::filesystem::remove(ingestedPath, ec);
    }
  };
  for (int i = 0; i < count; ++i) {
    wxVariant nameVar;
    trussTable->GetValue(nameVar, i, kTrussNameColumn);
    std::string name = std::string(nameVar.GetString().ToUTF8());
    if (name.empty())
      continue;
    if (static_cast<size_t>(i) >= trussPaths.size())
      continue;
    const std::string &path = trussPaths[i];
    if (path.empty())
      continue;
    std::string savedPath = path;
    if (std::filesystem::exists(path)) {
      const auto ingested = TrussAssetIngestion::Ingest(
          {PathUtils::PathFromUtf8(TrussDictionary::GetActiveDictionaryFilePath()),
           PathUtils::PathFromUtf8(ProjectUtils::GetWritableLibraryPath("trusses")) /
               "truss_dictionary.json",
           PathUtils::PathFromUtf8(path)});
      if (!ingested.success) {
        wxMessageBox("Could not ingest the truss file into dictionary-owned "
                     "GDTF storage. The dictionary was not saved.\n\n" +
                         wxString::FromUTF8(ingested.error),
                     "Save trusses dictionary", wxICON_ERROR | wxOK, this);
        cleanupIngestedPaths();
        return false;
      }
      if (!ingested.reusedExisting)
        ingestedPaths.push_back(ingested.finalPath);
      savedPath = ingested.finalPath.string();
    }
    rows.push_back({name, savedPath});
  }

  SortTrussRows(rows);
  std::unordered_map<std::string, std::string> dict;
  dict.reserve(rows.size());
  for (const auto &row : rows)
    dict[row.name] = row.path;
  std::string saveError;
  if (!TrussDictionary::Save(dict, &saveError)) {
    cleanupIngestedPaths();
    wxMessageBox("Could not save trusses dictionary.\n\n" +
                     wxString::FromUTF8(saveError),
                 "Save trusses dictionary", wxICON_ERROR | wxOK, this);
    return false;
  }

  LoadTrusses();
  return true;
}

// Adds a fixture or truss row to the selected dictionary page.
void DictionaryEditDialog::OnAdd(wxCommandEvent &WXUNUSED(event)) {
  const auto page = IsFixturesPage()
                        ? DictionaryEditorState::DictionaryEditorPage::Fixtures
                        : DictionaryEditorState::DictionaryEditorPage::Trusses;
  if (!EnsurePageCanWrite(page))
    return;

  if (IsFixturesPage()) {
    wxString fixDir =
        wxString::FromUTF8(ProjectUtils::GetWritableLibraryPath("fixtures"));
    wxFileDialog fdlg(this, "Select GDTF file", fixDir, wxEmptyString, "*.gdtf",
                      wxFD_OPEN | wxFD_FILE_MUST_EXIST);
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
    items.push_back(wxString::FromUTF8(
        std::filesystem::path(fullPath).filename().string()));
    items.push_back(wxString::FromUTF8(mode));
    items.push_back(wxString());
    items.push_back(wxString());
    fixtureTable->AppendItem(items);
    SetFixtureVisualColorCell(fixtureTable, fixtureTable->GetItemCount() - 1,
                              "");
    fixturePaths.push_back(fullPath);
  } else {
    wxString trussDir =
        wxString::FromUTF8(ProjectUtils::GetWritableLibraryPath("trusses"));
    wxFileDialog fdlg(this, "Select Truss file", trussDir, wxEmptyString,
                      "Truss files "
                      "(*.gdtf;*.gtruss;*.3ds;*.glb)|*.gdtf;*.gtruss;*.3ds;*."
                      "glb|All files|*.*",
                      wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (fdlg.ShowModal() != wxID_OK)
      return;
    wxString path = fdlg.GetPath();
    std::string fullPath = std::string(path.ToUTF8());
    std::string name = wxFileName(path).GetName().ToStdString();

    wxVector<wxVariant> items;
    items.push_back(wxString::FromUTF8(name));
    items.push_back(wxString::FromUTF8(
        std::filesystem::path(fullPath).filename().string()));
    trussTable->AppendItem(items);
    trussPaths.push_back(fullPath);
  }
}

// Deletes selected rows from the selected dictionary page.
void DictionaryEditDialog::OnDelete(wxCommandEvent &WXUNUSED(event)) {
  const auto page = IsFixturesPage()
                        ? DictionaryEditorState::DictionaryEditorPage::Fixtures
                        : DictionaryEditorState::DictionaryEditorPage::Trusses;
  if (!EnsurePageCanWrite(page))
    return;

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

void DictionaryEditDialog::UpdateFixtureVisualColorForFileAndMode(
    int row, const std::string &colorHex) {
  if (!fixtureTable || row < 0 ||
      static_cast<size_t>(row) >= fixturePaths.size())
    return;
  const std::string targetPath = fixturePaths[static_cast<size_t>(row)];
  if (targetPath.empty())
    return;

  wxVariant targetModeVar;
  fixtureTable->GetValue(targetModeVar, row, kFixtureModeColumn);
  const std::string targetModeKey =
      NormalizeFixtureModeKey(std::string(targetModeVar.GetString().ToUTF8()));

  const int rowCount = fixtureTable->GetItemCount();
  for (int i = 0; i < rowCount; ++i) {
    if (static_cast<size_t>(i) >= fixturePaths.size())
      continue;
    if (!FixturePathsMatchForColorFamily(fixturePaths[static_cast<size_t>(i)],
                                         targetPath))
      continue;
    wxVariant modeVar;
    fixtureTable->GetValue(modeVar, i, kFixtureModeColumn);
    const std::string modeKey =
        NormalizeFixtureModeKey(std::string(modeVar.GetString().ToUTF8()));
    if (modeKey != targetModeKey)
      continue;
    SetFixtureVisualColorCell(fixtureTable, i, colorHex);
  }
}

void DictionaryEditDialog::SyncFixtureVisualColorForFileAndMode(int row) {
  if (!fixtureTable || row < 0 ||
      static_cast<size_t>(row) >= fixturePaths.size())
    return;

  const std::string &targetPath = fixturePaths[static_cast<size_t>(row)];
  if (targetPath.empty())
    return;

  wxVariant targetModeVar;
  fixtureTable->GetValue(targetModeVar, row, kFixtureModeColumn);
  const std::string targetModeKey =
      NormalizeFixtureModeKey(std::string(targetModeVar.GetString().ToUTF8()));
  if (targetModeKey.empty())
    return;

  const int rowCount = fixtureTable->GetItemCount();
  for (int i = 0; i < rowCount; ++i) {
    if (i == row)
      continue;
    if (static_cast<size_t>(i) >= fixturePaths.size())
      continue;
    if (!FixturePathsMatchForColorFamily(fixturePaths[static_cast<size_t>(i)],
                                         targetPath)) {
      continue;
    }

    wxVariant modeVar;
    fixtureTable->GetValue(modeVar, i, kFixtureModeColumn);
    const std::string modeKey =
        NormalizeFixtureModeKey(std::string(modeVar.GetString().ToUTF8()));
    if (modeKey != targetModeKey)
      continue;

    wxVariant colorVar;
    fixtureTable->GetValue(colorVar, i, kFixtureVisualColorColumn);
    const std::string color = ExtractFixtureVisualColorText(colorVar);
    const auto normalizedColor = NormalizeFixtureHexColor(color);
    if (!normalizedColor.has_value() || normalizedColor->empty())
      continue;

    SetFixtureVisualColorCell(fixtureTable, row, *normalizedColor);
    return;
  }
}

// Propagates a fixture category edit to matching visible table rows.
void DictionaryEditDialog::UpdateFixtureCategoryForFile(
    int row, const std::string &category) {
  if (!fixtureTable || row < 0 ||
      static_cast<size_t>(row) >= fixturePaths.size())
    return;
  const std::string targetPath = fixturePaths[static_cast<size_t>(row)];
  if (targetPath.empty())
    return;

  const int rowCount = fixtureTable->GetItemCount();
  for (int i = 0; i < rowCount; ++i) {
    if (static_cast<size_t>(i) >= fixturePaths.size())
      continue;
    if (fixturePaths[static_cast<size_t>(i)] != targetPath)
      continue;

    fixtureTable->SetValue(wxVariant(wxString::FromUTF8(category)), i,
                           kFixtureCategoryColumn);
  }
}

// Saves changed dictionaries and closes only after every save succeeds.
void DictionaryEditDialog::OnOk(wxCommandEvent &WXUNUSED(event)) {
  const bool fixtureChanged = HasFixtureChanges();
  const bool trussChanged = HasTrussChanges();

  if (fixtureChanged && !SaveFixtures())
    return;
  if (trussChanged && !SaveTrusses())
    return;
  EndModal(wxID_OK);
}

// Imports into the active dictionary after resolving unsaved table edits.
void DictionaryEditDialog::OnImportDictionary(wxCommandEvent &WXUNUSED(event)) {
  if (!ConfirmDirtyChangesBeforeReload(
      IsFixturesPage() ? DictionaryEditorState::DictionaryEditorPage::Fixtures
                       : DictionaryEditorState::DictionaryEditorPage::Trusses,
      "importing a dictionary"))
    return;
  if (IsFixturesPage()) {
    (void)ImportFixturesDictionary();
    return;
  }
  (void)ImportTrussesDictionary();
}

// Imports entries into the active fixtures dictionary.
bool DictionaryEditDialog::ImportFixturesDictionary() {
  if (!EnsurePageCanWrite(DictionaryEditorState::DictionaryEditorPage::Fixtures))
    return false;

  const wxString fixturesDir =
      wxString::FromUTF8(ProjectUtils::GetWritableLibraryPath("fixtures"));
  wxFileDialog fileDialog(this, "Import fixtures dictionary", fixturesDir,
                          wxEmptyString,
                          "Dictionary files (*.json;*.zip)|*.json;*.zip|JSON "
                          "files (*.json)|*.json|ZIP files (*.zip)|*.zip",
                          wxFD_OPEN | wxFD_FILE_MUST_EXIST);
  if (fileDialog.ShowModal() != wxID_OK)
    return false;
  const std::string selectedPath = std::string(fileDialog.GetPath().ToUTF8());
  DictionaryBundle::PreparedImport preparedImport =
      DictionaryBundle::PrepareBundleImport(selectedPath,
                                            DictionaryBundle::Type::Fixtures);
  if (!preparedImport.errors.empty()) {
    DictionaryImportSummary errorSummary;
    errorSummary.errors = preparedImport.errors;
    wxMessageBox("Invalid fixtures dictionary file.\n\n" +
                     BuildSummaryText(errorSummary),
                 "Import fixtures dictionary", wxOK | wxICON_ERROR, this);
    DictionaryBundle::CleanupPreparedImport(preparedImport);
    return false;
  }
  const std::string importPath =
      preparedImport.is_bundle ? preparedImport.rewritten_snapshot_path.string()
                               : selectedPath;
  const auto validationPreview = GdtfDictionary::PreviewImportFromFile(
      importPath, DictionaryImportPolicy::AddMissing);
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

  const auto preview =
      GdtfDictionary::PreviewImportFromFile(importPath, policy);
  const auto pathValidation = ValidateFixtureImportPaths(importPath);
  wxString confirmText = "Policy:\n" + GetPolicyDescription(policy) +
                         "\n\nPreview summary:\n" + BuildSummaryText(preview) +
                         "\n\nApply import?";
  if (wxMessageBox(confirmText, "Confirm fixtures dictionary import",
                   wxYES_NO | wxICON_QUESTION, this) != wxYES) {
    DictionaryBundle::CleanupPreparedImport(preparedImport);
    return false;
  }
  if (!ConfirmImportMissingPaths(this, "Fixtures import warning",
                                 pathValidation)) {
    DictionaryBundle::CleanupPreparedImport(preparedImport);
    return false;
  }

  const auto result =
      preparedImport.is_bundle
          ? DictionaryBundle::ApplyPreparedBundleImport(preparedImport, policy).summary
          : GdtfDictionary::ApplyImportFromFile(importPath, policy);
  DictionaryBundle::CleanupPreparedImport(preparedImport);
  LoadFixtures();
  const bool hasErrors = result.HasErrors();
  wxMessageBox((hasErrors
                    ? "Fixtures dictionary import finished with errors.\n\n"
                    : "Fixtures dictionary import completed.\n\n") +
                   BuildSummaryText(result),
               "Fixtures dictionary import",
               wxOK | (hasErrors ? wxICON_WARNING : wxICON_INFORMATION), this);
  return !result.HasErrors();
}

// Imports entries into the active trusses dictionary.
bool DictionaryEditDialog::ImportTrussesDictionary() {
  if (!EnsurePageCanWrite(DictionaryEditorState::DictionaryEditorPage::Trusses))
    return false;

  const wxString trussesDir =
      wxString::FromUTF8(ProjectUtils::GetWritableLibraryPath("trusses"));
  wxFileDialog fileDialog(this, "Import trusses dictionary", trussesDir,
                          wxEmptyString,
                          "Dictionary files (*.json;*.zip)|*.json;*.zip|JSON "
                          "files (*.json)|*.json|ZIP files (*.zip)|*.zip",
                          wxFD_OPEN | wxFD_FILE_MUST_EXIST);
  if (fileDialog.ShowModal() != wxID_OK)
    return false;
  const std::string selectedPath = std::string(fileDialog.GetPath().ToUTF8());
  DictionaryBundle::PreparedImport preparedImport =
      DictionaryBundle::PrepareBundleImport(selectedPath,
                                            DictionaryBundle::Type::Trusses);
  if (!preparedImport.errors.empty()) {
    DictionaryImportSummary errorSummary;
    errorSummary.errors = preparedImport.errors;
    wxMessageBox("Invalid trusses dictionary file.\n\n" +
                     BuildSummaryText(errorSummary),
                 "Import trusses dictionary", wxOK | wxICON_ERROR, this);
    DictionaryBundle::CleanupPreparedImport(preparedImport);
    return false;
  }
  const std::string importPath =
      preparedImport.is_bundle ? preparedImport.rewritten_snapshot_path.string()
                               : selectedPath;
  const auto validationPreview = TrussDictionary::PreviewImportFromFile(
      importPath, DictionaryImportPolicy::AddMissing);
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

  const auto preview =
      TrussDictionary::PreviewImportFromFile(importPath, policy);
  const auto pathValidation = ValidateTrussImportPaths(importPath);
  wxString confirmText = "Policy:\n" + GetPolicyDescription(policy) +
                         "\n\nPreview summary:\n" + BuildSummaryText(preview) +
                         "\n\nApply import?";
  if (wxMessageBox(confirmText, "Confirm trusses dictionary import",
                   wxYES_NO | wxICON_QUESTION, this) != wxYES) {
    DictionaryBundle::CleanupPreparedImport(preparedImport);
    return false;
  }
  if (!ConfirmImportMissingPaths(this, "Trusses import warning",
                                 pathValidation)) {
    DictionaryBundle::CleanupPreparedImport(preparedImport);
    return false;
  }

  const auto result =
      preparedImport.is_bundle
          ? DictionaryBundle::ApplyPreparedBundleImport(preparedImport, policy).summary
          : TrussDictionary::ApplyImportFromFile(importPath, policy);
  DictionaryBundle::CleanupPreparedImport(preparedImport);
  LoadTrusses();
  const bool hasErrors = result.HasErrors();
  wxMessageBox((hasErrors
                    ? "Trusses dictionary import finished with errors.\n\n"
                    : "Trusses dictionary import completed.\n\n") +
                   BuildSummaryText(result),
               "Trusses dictionary import",
               wxOK | (hasErrors ? wxICON_WARNING : wxICON_INFORMATION), this);
  return !result.HasErrors();
}

// Opens the export format chooser and runs the selected export workflow.
void DictionaryEditDialog::OnExportDictionary(wxCommandEvent &WXUNUSED(event)) {
  const wxString choices[] = {"JSON Snapshot", "Portable ZIP Bundle"};
  wxSingleChoiceDialog dialog(
      this,
      "Export does not change the active dictionary. Choose the export format.",
      "Export dictionary", WXSIZEOF(choices), choices);
  if (dialog.ShowModal() != wxID_OK)
    return;

  const int selection = dialog.GetSelection();
  if (selection == 0) {
    if (IsFixturesPage()) {
      (void)ExportFixturesDictionary();
      return;
    }
    (void)ExportTrussesDictionary();
    return;
  }

  if (IsFixturesPage()) {
    (void)ExportFixturesPortableBundle();
    return;
  }
  (void)ExportTrussesPortableBundle();
}

// Resets the active dictionary after resolving unsaved table edits.
void DictionaryEditDialog::OnResetDictionary(wxCommandEvent &WXUNUSED(event)) {
  if (!ConfirmDirtyChangesBeforeReload(
      IsFixturesPage() ? DictionaryEditorState::DictionaryEditorPage::Fixtures
                       : DictionaryEditorState::DictionaryEditorPage::Trusses,
      "resetting a dictionary"))
    return;
  if (IsFixturesPage()) {
    (void)ResetFixturesDictionaryToDefault();
    return;
  }
  (void)ResetTrussesDictionaryToDefault();
}

// Exports the active fixtures dictionary as a JSON snapshot.
bool DictionaryEditDialog::ExportFixturesDictionary() {
  if (HasFixtureChanges() && !SaveFixtures())
    return false;

  auto dictOpt = GdtfDictionary::Load();
  if (!dictOpt) {
    wxMessageBox("Could not load fixtures dictionary for export.",
                 "Export fixtures dictionary", wxICON_ERROR | wxOK, this);
    return false;
  }

  const auto exportSummary = AnalyzeFixtureExportPaths(*dictOpt);
  if (!ConfirmExportReferences(this, "Export fixtures dictionary",
                               exportSummary))
    return false;

  const wxString fixturesDir =
      wxString::FromUTF8(ProjectUtils::GetWritableLibraryPath("fixtures"));
  wxFileDialog fileDialog(this, "Export fixtures dictionary", fixturesDir,
                          "gdtf_dictionary_snapshot.json",
                          "JSON files (*.json)|*.json",
                          wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if (fileDialog.ShowModal() != wxID_OK)
    return false;

  const bool copyReferencedAssets = false;
  const std::string outputPath = std::string(fileDialog.GetPath().ToUTF8());
  SnapshotExportResult exportResult;
  if (!SaveFixturesSnapshotToFile(this, outputPath, *dictOpt,
                                  copyReferencedAssets, exportResult)) {
    wxMessageBox("Could not write fixtures dictionary snapshot.",
                 "Export fixtures dictionary", wxICON_ERROR | wxOK, this);
    return false;
  }

  wxString info = "Fixtures dictionary snapshot exported successfully.";
  if (copyReferencedAssets) {
    info += "\nCopied assets: " +
            wxString::Format("%zu", exportResult.copied_assets);
    if (exportResult.missing_assets > 0) {
      info += "\nMissing assets: " +
              wxString::Format("%zu", exportResult.missing_assets);
      const size_t exampleCount =
          std::min<size_t>(exportResult.copy_errors.size(), 5);
      for (size_t i = 0; i < exampleCount; ++i)
        info += "\n- " + wxString::FromUTF8(exportResult.copy_errors[i]);
    }
  }
  wxMessageBox(info, "Export fixtures dictionary", wxICON_INFORMATION | wxOK,
               this);
  return true;
}

// Exports the active trusses dictionary as a JSON snapshot.
bool DictionaryEditDialog::ExportTrussesDictionary() {
  if (HasTrussChanges() && !SaveTrusses())
    return false;

  auto dictOpt = TrussDictionary::Load();
  if (!dictOpt) {
    wxMessageBox("Could not load trusses dictionary for export.",
                 "Export trusses dictionary", wxICON_ERROR | wxOK, this);
    return false;
  }

  const auto exportSummary = AnalyzeTrussExportPaths(*dictOpt);
  if (!ConfirmExportReferences(this, "Export trusses dictionary",
                               exportSummary))
    return false;

  const wxString trussesDir =
      wxString::FromUTF8(ProjectUtils::GetWritableLibraryPath("trusses"));
  wxFileDialog fileDialog(this, "Export trusses dictionary", trussesDir,
                          "truss_dictionary_snapshot.json",
                          "JSON files (*.json)|*.json",
                          wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if (fileDialog.ShowModal() != wxID_OK)
    return false;

  const bool copyReferencedAssets = false;
  const std::string outputPath = std::string(fileDialog.GetPath().ToUTF8());
  SnapshotExportResult exportResult;
  if (!SaveTrussesSnapshotToFile(this, outputPath, *dictOpt,
                                 copyReferencedAssets, exportResult)) {
    wxMessageBox("Could not write trusses dictionary snapshot.",
                 "Export trusses dictionary", wxICON_ERROR | wxOK, this);
    return false;
  }

  wxString info = "Trusses dictionary snapshot exported successfully.";
  if (copyReferencedAssets) {
    info += "\nCopied assets: " +
            wxString::Format("%zu", exportResult.copied_assets);
    if (exportResult.missing_assets > 0) {
      info += "\nMissing assets: " +
              wxString::Format("%zu", exportResult.missing_assets);
      const size_t exampleCount =
          std::min<size_t>(exportResult.copy_errors.size(), 5);
      for (size_t i = 0; i < exampleCount; ++i)
        info += "\n- " + wxString::FromUTF8(exportResult.copy_errors[i]);
    }
  }
  wxMessageBox(info, "Export trusses dictionary", wxICON_INFORMATION | wxOK,
               this);
  return true;
}

// Exports the active fixtures dictionary as a portable ZIP bundle.
bool DictionaryEditDialog::ExportFixturesPortableBundle() {
  if (HasFixtureChanges() && !SaveFixtures())
    return false;

  auto dictOpt = GdtfDictionary::Load();
  if (!dictOpt) {
    wxMessageBox("Could not load fixtures dictionary for portable export.",
                 "Export portable fixtures bundle", wxICON_ERROR | wxOK, this);
    return false;
  }

  const wxString fixturesDir =
      wxString::FromUTF8(ProjectUtils::GetWritableLibraryPath("fixtures"));
  wxFileDialog fileDialog(this, "Export portable fixtures bundle", fixturesDir,
                          "gdtf_dictionary_bundle.zip",
                          "ZIP files (*.zip)|*.zip",
                          wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if (fileDialog.ShowModal() != wxID_OK)
    return false;

  const std::string outputPath = std::string(fileDialog.GetPath().ToUTF8());
  std::string error;
  if (!DictionaryBundle::ExportFixturesBundle(*dictOpt, outputPath, error)) {
    wxMessageBox("Could not write fixtures portable bundle.\n" +
                     wxString::FromUTF8(error),
                 "Export portable fixtures bundle", wxICON_ERROR | wxOK, this);
    return false;
  }

  wxMessageBox("Fixtures portable bundle exported successfully.",
               "Export portable fixtures bundle", wxICON_INFORMATION | wxOK,
               this);
  return true;
}

// Exports the active trusses dictionary as a portable ZIP bundle.
bool DictionaryEditDialog::ExportTrussesPortableBundle() {
  if (HasTrussChanges() && !SaveTrusses())
    return false;

  auto dictOpt = TrussDictionary::Load();
  if (!dictOpt) {
    wxMessageBox("Could not load trusses dictionary for portable export.",
                 "Export portable trusses bundle", wxICON_ERROR | wxOK, this);
    return false;
  }

  const wxString trussesDir =
      wxString::FromUTF8(ProjectUtils::GetWritableLibraryPath("trusses"));
  wxFileDialog fileDialog(this, "Export portable trusses bundle", trussesDir,
                          "truss_dictionary_bundle.zip",
                          "ZIP files (*.zip)|*.zip",
                          wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if (fileDialog.ShowModal() != wxID_OK)
    return false;

  const std::string outputPath = std::string(fileDialog.GetPath().ToUTF8());
  std::string error;
  if (!DictionaryBundle::ExportTrussesBundle(*dictOpt, outputPath, error)) {
    wxMessageBox("Could not write trusses portable bundle.\n" +
                     wxString::FromUTF8(error),
                 "Export portable trusses bundle", wxICON_ERROR | wxOK, this);
    return false;
  }

  wxMessageBox("Trusses portable bundle exported successfully.",
               "Export portable trusses bundle", wxICON_INFORMATION | wxOK,
               this);
  return true;
}

// Resets the active fixtures dictionary contents to application defaults.
bool DictionaryEditDialog::ResetFixturesDictionaryToDefault() {
  if (!EnsurePageCanWrite(DictionaryEditorState::DictionaryEditorPage::Fixtures))
    return false;

  if (wxMessageBox(
          "Reset active fixtures dictionary to application defaults?\n"
          "Current entries will be replaced after a backup is created.",
          "Reset fixtures dictionary", wxYES_NO | wxNO_DEFAULT | wxICON_WARNING,
          this) != wxYES) {
    return false;
  }

  const std::filesystem::path activePath =
      PathUtils::PathFromUtf8(GdtfDictionary::GetActiveDictionaryFilePath());
  const std::filesystem::path basePath =
      ProjectUtils::GetBaseLibraryPath("fixtures") / "gdtf_dictionary.json";
  const auto result = DictionaryResetService::ResetToDefaults(
      {ActiveDictionaryStorage::DictionaryKind::Fixtures, activePath,
       PathUtils::PathFromUtf8(ProjectUtils::GetWritableLibraryPath("fixtures")) /
           "gdtf_dictionary.json",
       basePath});
  LoadFixtures();
  const bool hasErrors = result.HasErrors() || result.missing_files_count > 0;
  wxMessageBox((hasErrors
                    ? "Fixtures dictionary reset finished with issues.\n\n"
                    : "Fixtures dictionary restored to defaults.\n\n") +
                   BuildSummaryText(result),
               "Reset fixtures dictionary",
               wxOK | (hasErrors ? wxICON_WARNING : wxICON_INFORMATION), this);
  return !result.HasErrors();
}

// Resets the active trusses dictionary contents to application defaults.
bool DictionaryEditDialog::ResetTrussesDictionaryToDefault() {
  if (!EnsurePageCanWrite(DictionaryEditorState::DictionaryEditorPage::Trusses))
    return false;

  if (wxMessageBox(
          "Reset active trusses dictionary to application defaults?\n"
          "Current entries will be replaced after a backup is created.",
          "Reset trusses dictionary", wxYES_NO | wxNO_DEFAULT | wxICON_WARNING,
          this) != wxYES) {
    return false;
  }

  const std::filesystem::path activePath =
      PathUtils::PathFromUtf8(TrussDictionary::GetActiveDictionaryFilePath());
  const std::filesystem::path basePath =
      ProjectUtils::GetBaseLibraryPath("trusses") / "truss_dictionary.json";
  const auto result = DictionaryResetService::ResetToDefaults(
      {ActiveDictionaryStorage::DictionaryKind::Trusses, activePath,
       PathUtils::PathFromUtf8(ProjectUtils::GetWritableLibraryPath("trusses")) /
           "truss_dictionary.json",
       basePath});
  LoadTrusses();
  const bool hasErrors = result.HasErrors() || result.missing_files_count > 0;
  wxMessageBox((hasErrors ? "Trusses dictionary reset finished with issues.\n\n"
                          : "Trusses dictionary restored to defaults.\n\n") +
                   BuildSummaryText(result),
               "Reset trusses dictionary",
               wxOK | (hasErrors ? wxICON_WARNING : wxICON_INFORMATION), this);
  return !result.HasErrors();
}

// Opens the page-specific editor for the activated table cell.
void DictionaryEditDialog::OnItemActivated(wxDataViewEvent &event) {
  const auto page = IsFixturesPage()
                        ? DictionaryEditorState::DictionaryEditorPage::Fixtures
                        : DictionaryEditorState::DictionaryEditorPage::Trusses;
  if (!EnsurePageCanWrite(page))
    return;

  wxDataViewListCtrl *table = IsFixturesPage() ? fixtureTable : trussTable;
  if (!table)
    return;
  wxDataViewItem item = event.GetItem();
  int row = table->ItemToRow(item);
  if (row == wxNOT_FOUND)
    return;
  int col = event.GetColumn();
  if (IsFixturesPage()) {
    if (col == kFixtureModeColumn) {
      if (static_cast<size_t>(row) >= fixturePaths.size())
        return;
      std::string fullPath = fixturePaths[row];
      if (fullPath.empty() || !std::filesystem::exists(fullPath)) {
        table->SetValue(wxVariant(wxString()), row, kFixtureModeColumn);
        return;
      }
      auto modes = GetSortedModes(fullPath);
      if (modes.empty()) {
        table->SetValue(wxVariant(wxString()), row, kFixtureModeColumn);
        return;
      }
      wxArrayString choices;
      for (const auto &m : modes)
        choices.push_back(wxString::FromUTF8(m));
      wxSingleChoiceDialog dlg(this, "Select DMX mode", "DMX Mode", choices);
      if (dlg.ShowModal() == wxID_OK) {
        std::string mode = std::string(dlg.GetStringSelection().ToUTF8());
        table->SetValue(wxVariant(wxString::FromUTF8(mode)), row,
                        kFixtureModeColumn);
        SyncFixtureVisualColorForFileAndMode(row);
      }
      return;
    }
    if (col == kFixtureCategoryColumn) {
      wxVariant current;
      table->GetValue(current, row, col);
      const wxArrayString choices = {
          "Beam",   "Blinder", "Conventional", "FX",    "Hoist",
          "Hybrid", "Laser",   "LED",          "Smoke", "Spot",
          "Strobe", "Unknown", "Video",        "Wash"};
      wxSingleChoiceDialog dlg(this, "Select category", "Category", choices);
      if (!current.GetString().empty()) {
        const int currentSelection = choices.Index(current.GetString());
        if (currentSelection != wxNOT_FOUND)
          dlg.SetSelection(currentSelection);
      }
      if (dlg.ShowModal() != wxID_OK)
        return;
      const std::string selectedCategory =
          GdtfFixtureCategory::NormalizeCategory(
              std::string(dlg.GetStringSelection().ToUTF8()));
      if (selectedCategory.empty())
        return;
      UpdateFixtureCategoryForFile(row, selectedCategory);
      return;
    }
    if (col == kFixtureVisualColorColumn) {
      wxVariant current;
      table->GetValue(current, row, col);
      wxString colorString =
          wxString::FromUTF8(ExtractFixtureVisualColorText(current));
      wxColour initial(colorString);
      if (colorString.IsEmpty() || !initial.IsOk())
        initial = *wxWHITE;
      wxColourData data;
      data.SetColour(initial);
      wxColourDialog dlg(this, &data);
      if (dlg.ShowModal() != wxID_OK)
        return;
      const wxColour selected = dlg.GetColourData().GetColour();
      const std::string hex =
          wxString::Format("#%02X%02X%02X", selected.Red(), selected.Green(),
                           selected.Blue())
              .ToStdString();
      UpdateFixtureVisualColorForFileAndMode(row, hex);
      return;
    }
    if (col != kFixtureFileColumn)
      return;
    wxFileDialog fdlg(
        this, "Select GDTF file",
        wxString::FromUTF8(ProjectUtils::GetWritableLibraryPath("fixtures")),
        wxEmptyString, "*.gdtf", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (fdlg.ShowModal() != wxID_OK)
      return;
    wxString path = fdlg.GetPath();
    std::string fullPath = std::string(path.ToUTF8());
    if (static_cast<size_t>(row) >= fixturePaths.size())
      fixturePaths.resize(row + 1);
    fixturePaths[row] = fullPath;
    table->SetValue(wxVariant(wxString::FromUTF8(
                        std::filesystem::path(fullPath).filename().string())),
                    row, kFixtureFileColumn);

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
    table->SetValue(wxVariant(wxString::FromUTF8(mode)), row,
                    kFixtureModeColumn);
    SyncFixtureVisualColorForFileAndMode(row);
  } else {
    if (col != kTrussFileColumn)
      return;
    wxFileDialog fdlg(
        this, "Select Truss file",
        wxString::FromUTF8(ProjectUtils::GetWritableLibraryPath("trusses")),
        wxEmptyString,
        "Truss files "
        "(*.gdtf;*.gtruss;*.3ds;*.glb)|*.gdtf;*.gtruss;*.3ds;*.glb|All "
        "files|*.*",
        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (fdlg.ShowModal() != wxID_OK)
      return;
    wxString path = fdlg.GetPath();
    std::string fullPath = std::string(path.ToUTF8());
    if (static_cast<size_t>(row) >= trussPaths.size())
      trussPaths.resize(row + 1);
    trussPaths[row] = fullPath;
    table->SetValue(wxVariant(wxString::FromUTF8(
                        std::filesystem::path(fullPath).filename().string())),
                    row, kTrussFileColumn);
  }
}
