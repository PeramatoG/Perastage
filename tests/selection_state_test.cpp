#include "configservices.h"

#include <cassert>

int main() {
  SelectionState state;
  state.SetSelectedFixtures({"f1", "f2"});
  state.SetSelectedTrusses({"t1"});
  state.SetSelectedSupports({"s1"});
  state.SetSelectedSceneObjects({"o1"});

  assert(state.GetSelectedFixtures().size() == 2);
  assert(state.GetSelectedTrusses().size() == 1);
  assert(state.GetSelectedSupports().size() == 1);
  assert(state.GetSelectedSceneObjects().size() == 1);

  state.Clear();
  assert(state.GetSelectedFixtures().empty());
  assert(state.GetSelectedTrusses().empty());
  assert(state.GetSelectedSupports().empty());
  assert(state.GetSelectedSceneObjects().empty());
  return 0;
}
