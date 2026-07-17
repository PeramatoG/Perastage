#include "dictionary_editor_state.h"

#include <cassert>
#include <string>
#include <vector>

namespace {
using DictionaryEditorState::DictionaryEditorPage;
using DictionaryEditorState::DirtyGuardChoice;
using DictionaryEditorState::DirtyGuardResult;
using DictionaryEditorState::FixtureSnapshotRow;
using DictionaryEditorState::TrussSnapshotRow;

// Verifies fixture snapshots include category edits.
void FixtureCategoryChangesAreDirty() {
  std::vector<FixtureSnapshotRow> before = {
      {"Fixture", "/missing/fixture.gdtf", "Mode", "Spot", "#112233"}};
  std::vector<FixtureSnapshotRow> after = before;
  after[0].category = "Wash";
  assert(DictionaryEditorState::BuildFixtureSnapshot(before) !=
         DictionaryEditorState::BuildFixtureSnapshot(after));
}

// Verifies deleting an unresolved fixture entry changes the snapshot.
void DeletingMissingFixtureEntryIsDirty() {
  std::vector<FixtureSnapshotRow> before = {
      {"Missing A", "/missing/a.gdtf", "Mode", "Spot", "#112233"},
      {"Missing B", "/missing/b.gdtf", "Mode", "Wash", "#445566"}};
  std::vector<FixtureSnapshotRow> after = {before[1]};
  assert(DictionaryEditorState::BuildFixtureSnapshot(before) !=
         DictionaryEditorState::BuildFixtureSnapshot(after));
}

// Verifies deleting one unresolved truss entry changes the snapshot.
void DeletingMissingTrussEntryIsDirty() {
  std::vector<TrussSnapshotRow> before = {
      {"Missing A", "/missing/a.gdtf"}, {"Missing B", "/missing/b.gdtf"}};
  std::vector<TrussSnapshotRow> after = {before[1]};
  assert(DictionaryEditorState::BuildTrussSnapshot(before) !=
         DictionaryEditorState::BuildTrussSnapshot(after));
}

// Verifies unresolved fixture entries remain represented next to edited rows.
void UnresolvedFixtureEntriesRemainRepresented() {
  std::vector<FixtureSnapshotRow> rows = {
      {"Missing A", "/missing/a.gdtf", "Mode A", "Spot", "#112233"},
      {"Edited B", "/existing/b.gdtf", "Mode B", "Wash", "#445566"}};
  auto snapshot = DictionaryEditorState::BuildFixtureSnapshot(rows);
  assert(snapshot.size() == 2);
  assert(snapshot[0].find("Missing A") != std::string::npos ||
         snapshot[1].find("Missing A") != std::string::npos);
}


// Verifies fixture operations do not inspect truss dirty state.
void FixtureGuardIgnoresDirtyTrusses() {
  assert(!DictionaryEditorState::HasPageChanges(DictionaryEditorPage::Fixtures,
                                                false, true));
  assert(DictionaryEditorState::HasPageChanges(DictionaryEditorPage::Fixtures,
                                               true, false));
}

// Verifies truss operations do not inspect fixture dirty state.
void TrussGuardIgnoresDirtyFixtures() {
  assert(!DictionaryEditorState::HasPageChanges(DictionaryEditorPage::Trusses,
                                                true, false));
  assert(DictionaryEditorState::HasPageChanges(DictionaryEditorPage::Trusses,
                                               false, true));
}

// Verifies discard can continue without implying a save succeeded.
void DiscardDoesNotRequireSaveSuccess() {
  assert(DictionaryEditorState::ResolveDirtyGuard(
             true, DirtyGuardChoice::Discard, false) ==
         DirtyGuardResult::Continue);
}

// Verifies invalid active dictionary statuses block normal writes.
void InvalidDictionaryStatusRequiresRecovery() {
  assert(DictionaryEditorState::RequiresWriteRecovery({true, false, false}));
  assert(DictionaryEditorState::RequiresWriteRecovery({false, true, false}));
  assert(DictionaryEditorState::RequiresWriteRecovery({false, false, true}));
  assert(!DictionaryEditorState::RequiresWriteRecovery({false, false, false}));
}

// Verifies save sequencing stops after the first changed-page failure.
void FailedSavePreventsLaterSaveCallbacks() {
  int calls = 0;
  bool previousSucceeded = true;
  auto saveFirst = [&] {
    ++calls;
    return false;
  };
  auto saveSecond = [&] {
    ++calls;
    return true;
  };
  if (DictionaryEditorState::ShouldRunPageSave(previousSucceeded, true))
    previousSucceeded = saveFirst();
  if (DictionaryEditorState::ShouldRunPageSave(previousSucceeded, true))
    previousSucceeded = saveSecond();
  assert(calls == 1);
}

// Verifies a failed dirty-save decision aborts the close or reload path.
void FailedSaveAbortsGuardedPath() {
  assert(DictionaryEditorState::ResolveDirtyGuard(
             true, DirtyGuardChoice::Save, false) == DirtyGuardResult::Abort);
  assert(DictionaryEditorState::ResolveDirtyGuard(
             true, DirtyGuardChoice::Save, true) == DirtyGuardResult::Continue);
}

// Verifies discard and cancel decisions can be tested without native dialogs.
void DirtyGuardDecisionHandlingIsDialogIndependent() {
  assert(DictionaryEditorState::ResolveDirtyGuard(
             true, DirtyGuardChoice::Discard, false) ==
         DirtyGuardResult::Continue);
  assert(DictionaryEditorState::ResolveDirtyGuard(
             true, DirtyGuardChoice::Cancel, true) == DirtyGuardResult::Abort);
  assert(DictionaryEditorState::ResolveDirtyGuard(
             false, DirtyGuardChoice::Cancel, false) ==
         DirtyGuardResult::Continue);
}

} // namespace

// Runs Dictionary Editor state regression checks.
int main() {
  FixtureCategoryChangesAreDirty();
  DeletingMissingFixtureEntryIsDirty();
  DeletingMissingTrussEntryIsDirty();
  UnresolvedFixtureEntriesRemainRepresented();
  FixtureGuardIgnoresDirtyTrusses();
  TrussGuardIgnoresDirtyFixtures();
  DiscardDoesNotRequireSaveSuccess();
  InvalidDictionaryStatusRequiresRecovery();
  FailedSavePreventsLaterSaveCallbacks();
  FailedSaveAbortsGuardedPath();
  DirtyGuardDecisionHandlingIsDialogIndependent();
  return 0;
}
