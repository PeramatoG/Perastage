#include "configservices.h"

#include <cassert>

// Verifies scene history restoration for standard and continuous placement flows.
int main() {
  HistoryManager history;
  SelectionState selection;
  ProjectSession session;

  Fixture f;
  f.uuid = "f1";
  session.GetScene().fixtures[f.uuid] = f;
  selection.SetSelectedFixtures({"f1"});

  history.PushUndoState(session.GetScene(), selection, "add fixture");
  session.GetScene().fixtures.clear();
  selection.Clear();

  assert(history.CanUndo());
  assert(history.Undo(session.GetScene(), selection) == "add fixture");
  assert(session.GetScene().fixtures.size() == 1);
  assert(selection.GetSelectedFixtures().size() == 1);
  assert(history.CanRedo());

  std::optional<std::string> restoredOverrides;
  assert(history.Redo(session.GetScene(), selection, nullptr, nullptr,
                      &restoredOverrides) == "add fixture");

  HistoryManager metadataHistory;
  metadataHistory.PushUndoState(session.GetScene(), selection, "paste fixture",
                                std::nullopt, nullptr,
                                std::string("{\"f1\":{\"x\":4}}"));
  std::optional<std::string> metadata =
      std::string("{\"pasted\":{\"x\":4}}");
  assert(metadataHistory.Undo(session.GetScene(), selection, nullptr, nullptr,
                              &metadata) == "paste fixture");
  assert(metadata == "{\"f1\":{\"x\":4}}");
  assert(metadataHistory.Redo(session.GetScene(), selection, nullptr, nullptr,
                              &metadata) == "paste fixture");
  assert(metadata == "{\"pasted\":{\"x\":4}}");

  HistoryManager placementHistory;
  SelectionState placementSelection;
  MvrScene placementScene;
  placementHistory.PushUndoState(placementScene, placementSelection,
                                 "add fixture");

  Fixture firstPending;
  firstPending.uuid = "pending-1";
  placementScene.fixtures[firstPending.uuid] = firstPending;
  placementHistory.PushUndoState(placementScene, placementSelection,
                                 "place fixture");

  Fixture secondPending = firstPending;
  secondPending.uuid = "pending-2";
  placementScene.fixtures[secondPending.uuid] = secondPending;

  assert(placementHistory.Undo(placementScene, placementSelection) ==
         "place fixture");
  assert(placementScene.fixtures.size() == 1);
  assert(placementScene.fixtures.contains(firstPending.uuid));
  assert(!placementScene.fixtures.contains(secondPending.uuid));

  assert(placementHistory.Undo(placementScene, placementSelection) ==
         "add fixture");
  assert(placementScene.fixtures.empty());
  return 0;
}
