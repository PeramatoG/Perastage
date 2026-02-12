#include "configservices.h"

#include <cassert>

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

  assert(history.Redo(session.GetScene(), selection) == "add fixture");
  return 0;
}
