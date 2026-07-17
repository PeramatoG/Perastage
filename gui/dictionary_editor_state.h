#pragma once

#include <algorithm>
#include <string>
#include <vector>

namespace DictionaryEditorState {

struct FixtureSnapshotRow {
  std::string name;
  std::string path;
  std::string mode;
  std::string category;
  std::string visualColorHex;
};

struct TrussSnapshotRow {
  std::string name;
  std::string path;
};

enum class DictionaryEditorPage { Fixtures, Trusses };

enum class DirtyGuardChoice { Save, Discard, Cancel };

enum class DirtyGuardResult { Continue, Abort };

struct LoadRecoveryStatus {
  bool activeDictionaryInvalid = false;
  bool activeDictionaryMissing = false;
  bool temporaryFallbackUsed = false;
};

// Builds a stable fixture snapshot from all user-editable dictionary fields.
inline std::vector<std::string>
BuildFixtureSnapshot(std::vector<FixtureSnapshotRow> rows) {
  std::vector<std::string> snapshot;
  snapshot.reserve(rows.size());
  for (const auto &row : rows) {
    if (row.name.empty())
      continue;
    snapshot.push_back(row.name + '\n' + row.path + '\n' + row.mode + '\n' +
                       row.category + '\n' + row.visualColorHex);
  }
  std::sort(snapshot.begin(), snapshot.end());
  return snapshot;
}

// Builds a stable truss snapshot from all user-editable dictionary fields.
inline std::vector<std::string>
BuildTrussSnapshot(std::vector<TrussSnapshotRow> rows) {
  std::vector<std::string> snapshot;
  snapshot.reserve(rows.size());
  for (const auto &row : rows) {
    if (row.name.empty())
      continue;
    snapshot.push_back(row.name + '\n' + row.path);
  }
  std::sort(snapshot.begin(), snapshot.end());
  return snapshot;
}

// Resolves whether an operation guarded by dirty state should continue.
inline DirtyGuardResult ResolveDirtyGuard(bool hasChanges,
                                          DirtyGuardChoice choice,
                                          bool saveSucceeded) {
  if (!hasChanges)
    return DirtyGuardResult::Continue;
  if (choice == DirtyGuardChoice::Cancel)
    return DirtyGuardResult::Abort;
  if (choice == DirtyGuardChoice::Discard)
    return DirtyGuardResult::Continue;
  return saveSucceeded ? DirtyGuardResult::Continue : DirtyGuardResult::Abort;
}

// Returns true when writes must wait for explicit dictionary recovery.
inline bool RequiresWriteRecovery(const LoadRecoveryStatus &status) {
  return status.activeDictionaryInvalid || status.activeDictionaryMissing ||
         status.temporaryFallbackUsed;
}

// Resolves whether a guarded page has unsaved edits.
inline bool HasPageChanges(DictionaryEditorPage page, bool fixtureChanged,
                           bool trussChanged) {
  return page == DictionaryEditorPage::Fixtures ? fixtureChanged : trussChanged;
}

// Returns true when a changed page save should run after earlier saves.
inline bool ShouldRunPageSave(bool previousSavesSucceeded, bool pageChanged) {
  return previousSavesSucceeded && pageChanged;
}

} // namespace DictionaryEditorState
