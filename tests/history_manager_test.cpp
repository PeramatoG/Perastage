#include "configservices.h"

#include <cassert>

namespace {
// Exercises combined project states, absent metadata, empty metadata and branching.
void TestLayoutHistory() {
  HistoryManager history;
  MvrScene scene;
  SelectionState selection;
  LayerVisibilityState layers;
  std::optional<std::string> layout;
  std::optional<std::string> labels;
  const std::optional<std::string> values[]{std::nullopt, "", "{\"layouts\":[]}"};
  const auto setState = [&](int index) {
    scene.fixtures.clear();
    Fixture fixture;
    fixture.uuid = std::to_string(index);
    scene.fixtures[fixture.uuid] = fixture;
    selection.SetSelectedFixtures({fixture.uuid});
    selection.SetSelectedTrusses({"truss-" + fixture.uuid});
    selection.SetSelectedSupports({"support-" + fixture.uuid});
    selection.SetSelectedSceneObjects({"object-" + fixture.uuid});
    layers.SetHiddenLayers({fixture.uuid});
    layers.SetCurrentLayer(fixture.uuid);
    layout = values[index];
    labels = "labels-" + fixture.uuid;
  };
  const auto check = [&](int index) {
    const auto id = std::to_string(index);
    assert(scene.fixtures.size() == 1 && scene.fixtures.contains(id));
    assert(selection.GetSelectedFixtures() == std::vector<std::string>{id});
    assert(selection.GetSelectedTrusses() == std::vector<std::string>{"truss-" + id});
    assert(selection.GetSelectedSupports() == std::vector<std::string>{"support-" + id});
    assert(selection.GetSelectedSceneObjects() == std::vector<std::string>{"object-" + id});
    assert(layers.GetHiddenLayers() == std::unordered_set<std::string>{id});
    assert(layers.GetCurrentLayer() == id);
    assert(layout == values[index]);
    assert(labels == "labels-" + id);
  };
  setState(0);
  for (int next = 1; next <= 2; ++next) {
    history.PushUndoState(scene, selection, "layout change", layout, &layers, labels);
    setState(next);
  }
  for (int cycle = 0; cycle < 3; ++cycle) {
    for (int previous : {1, 0}) {
      assert(history.Undo(scene, selection, &layout, &layers, &labels) == "layout change");
      check(previous);
    }
    assert(!history.CanUndo());
    for (int next : {1, 2}) {
      assert(history.Redo(scene, selection, &layout, &layers, &labels) == "layout change");
      check(next);
    }
    assert(!history.CanRedo());
  }
  history.Undo(scene, selection, &layout, &layers, &labels);
  history.PushUndoState(scene, selection, "new branch", layout, &layers, labels);
  layout = "replacement";
  assert(!history.CanRedo());
  history.Undo(scene, selection, &layout, &layers, &labels);
  check(1);
  history.Redo(scene, selection, &layout, &layers, &labels);
  assert(layout == "replacement");
}
// Interleaves scene-only and document-only operations in one history.
void TestInterleavedHistory() {
  HistoryManager history;
  MvrScene scene;
  SelectionState selection;
  std::optional<std::string> layout = "A";
  history.PushUndoState(scene, selection, "scene add", layout);
  Fixture fixture;
  fixture.uuid = "fixture";
  scene.fixtures[fixture.uuid] = fixture;
  history.PushUndoState(scene, selection, "layout edit", layout);
  layout = "B";
  history.PushUndoState(scene, selection, "scene delete", layout);
  scene.fixtures.clear();
  for (int cycle = 0; cycle < 2; ++cycle) {
    assert(history.Undo(scene, selection, &layout) == "scene delete");
    assert(scene.fixtures.size() == 1 && layout == "B");
    assert(history.Undo(scene, selection, &layout) == "layout edit");
    assert(scene.fixtures.size() == 1 && layout == "A");
    assert(history.Undo(scene, selection, &layout) == "scene add");
    assert(scene.fixtures.empty() && layout == "A");
    assert(history.Redo(scene, selection, &layout) == "scene add");
    assert(scene.fixtures.size() == 1 && layout == "A");
    assert(history.Redo(scene, selection, &layout) == "layout edit");
    assert(scene.fixtures.size() == 1 && layout == "B");
    assert(history.Redo(scene, selection, &layout) == "scene delete");
    assert(scene.fixtures.empty() && layout == "B");
  }
}
} // namespace

// Verifies scene history restoration for standard and continuous placement flows.
int main() {
  TestLayoutHistory();
  TestInterleavedHistory();
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

  HistoryManager clipboardHistory;
  SelectionState clipboardSelection;
  MvrScene clipboardScene;
  const std::vector<std::string> confirmedUuids{"paste-a", "paste-b",
                                                "paste-c"};
  for (const auto &uuid : confirmedUuids) {
    clipboardHistory.PushUndoState(clipboardScene, clipboardSelection,
                                   "paste scene element");
    Fixture pasted;
    pasted.uuid = uuid;
    clipboardScene.fixtures[uuid] = pasted;
    clipboardSelection.SetSelectedFixtures({uuid});
  }
  Fixture provisional;
  provisional.uuid = "paste-d";
  const MvrScene beforeProvisional = clipboardScene;
  const SelectionState selectionBeforeProvisional = clipboardSelection;
  clipboardScene.fixtures[provisional.uuid] = provisional;
  clipboardSelection.SetSelectedFixtures({provisional.uuid});
  clipboardScene = beforeProvisional;
  clipboardSelection = selectionBeforeProvisional;
  assert(!clipboardScene.fixtures.contains(provisional.uuid));
  assert(clipboardSelection.GetSelectedFixtures() ==
         std::vector<std::string>{"paste-c"});

  for (auto it = confirmedUuids.rbegin(); it != confirmedUuids.rend(); ++it) {
    assert(clipboardHistory.Undo(clipboardScene, clipboardSelection) ==
           "paste scene element");
    assert(!clipboardScene.fixtures.contains(*it));
  }
  assert(!clipboardHistory.CanUndo());
  for (const auto &uuid : confirmedUuids) {
    assert(clipboardHistory.Redo(clipboardScene, clipboardSelection) ==
           "paste scene element");
    assert(clipboardScene.fixtures.contains(uuid));
    assert(clipboardSelection.GetSelectedFixtures() ==
           std::vector<std::string>{uuid});
  }
  return 0;
}
