#include "configservices.h"

#include <cassert>

int main() {
  SelectionState state;
  state.SetSelectedFixtures({"f1", "f2"});
  state.SetSelectedTrusses({"t1"});
  state.SetSelectedSupports({"s1"});
  state.SetSelectedSceneObjects({"o1"});
  state.SetSelectedGroupObjects({"g1"});
  assert(state.GetSelectedGroupObjects() == std::vector<std::string>{"g1"});

  assert(state.GetSelectedFixtures().size() == 2);
  assert(state.GetSelectedTrusses().size() == 1);
  assert(state.GetSelectedSupports().size() == 1);
  assert(state.GetSelectedSceneObjects().size() == 1);

  assert(state.GetOrderedSelection().front().uuid == "f1");
  assert(state.GetOrderedSelection().back().uuid == "g1");
  state.SetSelectedFixtures({"f2","f1"}); // A sorted UI refresh preserves chronology.
  assert(state.GetOrderedSelection().front().uuid == "f1");
  state.SetSelectedFixtures({"f2"});
  state.SetSelectedFixtures({"f2","f1"});
  assert(state.GetOrderedSelection().back().uuid == "f1");
  const auto order = state.GetOrderedSelection();
  state.Clear();
  state.SetSelectedTrusses({"t1"});
  state.SetSelectedFixtures({"f1","f2"});
  state.RestoreOrderedSelection(order);
  assert(state.GetOrderedSelection().front().uuid == "f2");
  assert(state.GetOrderedSelection()[1].uuid == "t1");
  assert(state.GetOrderedSelection().back().uuid == "f1");
  MvrScene scene;
  Fixture existing; existing.uuid="f1"; scene.fixtures["f1"]=existing;
  state.Prune(scene);
  assert(state.GetOrderedSelection().size() == 1 && state.GetOrderedSelection()[0].uuid == "f1");
  assert(state.GetSelectedTrusses().empty());
  state.Clear();
  assert(state.GetSelectedFixtures().empty());
  assert(state.GetSelectedTrusses().empty());
  assert(state.GetSelectedSupports().empty());
  assert(state.GetSelectedSceneObjects().empty());
  assert(state.GetSelectedGroupObjects().empty());
  return 0;
}
