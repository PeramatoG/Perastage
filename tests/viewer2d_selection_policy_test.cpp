#include "../viewer2d/interaction/viewer2d_selection_policy.h"

#include <cassert>

using namespace viewer2d::interaction;

// Returns the typed bucket corresponding to a scene-element kind.
static const std::vector<std::string> &Bucket(const SelectionBuckets &buckets,
                                              SceneElementKind kind) {
  if (kind == SceneElementKind::Fixture)
    return buckets.fixtures;
  if (kind == SceneElementKind::Truss)
    return buckets.trusses;
  if (kind == SceneElementKind::Support)
    return buckets.supports;
  return buckets.sceneObjects;
}

// Verifies click modifiers, cross-table merging, clearing, and drag
// preparation.
int main() {
  for (const auto kind :
       {SceneElementKind::Fixture, SceneElementKind::Truss,
        SceneElementKind::Support, SceneElementKind::SceneObject}) {
    const auto replacement = Viewer2DSelectionPolicy::DecideClick(
        {true, kind, "clicked", false, false, false, {}});
    assert(Bucket(replacement.selection, kind) ==
           std::vector<std::string>{"clicked"});

    SelectionBuckets current;
    if (kind == SceneElementKind::Fixture)
      current.fixtures = {"existing"};
    else if (kind == SceneElementKind::Truss)
      current.trusses = {"existing"};
    else if (kind == SceneElementKind::Support)
      current.supports = {"existing"};
    else
      current.sceneObjects = {"existing"};
    const auto added = Viewer2DSelectionPolicy::DecideClick(
        {true, kind, "clicked", true, false, false, current});
    assert(Bucket(added.selection, kind).size() == 2);
    const auto removed = Viewer2DSelectionPolicy::DecideClick(
        {true, kind, "existing", true, false, false, current});
    assert(Bucket(removed.selection, kind).empty());
    const auto controlKept = Viewer2DSelectionPolicy::DecideClick(
        {true, kind, "existing", false, true, false, current});
    assert(Bucket(controlKept.selection, kind).size() == 1);
    const auto controlAdded = Viewer2DSelectionPolicy::DecideClick(
        {true, kind, "clicked", false, true, false, current});
    assert(Bucket(controlAdded.selection, kind).size() == 2);
  }

  SelectionBuckets mixed{{"fixture"}, {"truss"}, {"support"}, {"object"}};
  const auto cross = Viewer2DSelectionPolicy::DecideClick(
      {true, SceneElementKind::Truss, "new-truss", false, false, true, mixed});
  assert(cross.selection.fixtures == std::vector<std::string>{"fixture"});
  assert(cross.selection.trusses == std::vector<std::string>{"new-truss"});
  assert(
      cross.viewerSelection ==
      (std::vector<std::string>{"fixture", "new-truss", "support", "object"}));
  ClickSelectionInput emptyClick;
  assert(Viewer2DSelectionPolicy::DecideClick(emptyClick).clearAll);

  for (const auto target : {DragTarget::Fixtures, DragTarget::Trusses,
                            DragTarget::Supports, DragTarget::SceneObjects}) {
    SelectionBuckets selected;
    if (target == DragTarget::Fixtures)
      selected.fixtures = {"a", "b"};
    else if (target == DragTarget::Trusses)
      selected.trusses = {"a", "b"};
    else if (target == DragTarget::Supports)
      selected.supports = {"a", "b"};
    else
      selected.sceneObjects = {"a", "b"};
    const auto existing =
        Viewer2DSelectionPolicy::PrepareDrag(target, "a", selected);
    assert(existing.valid && existing.usesCurrentSelection);
    assert(existing.activeUuids == (std::vector<std::string>{"a", "b"}));
    const auto unselected =
        Viewer2DSelectionPolicy::PrepareDrag(target, "new", selected);
    assert(!unselected.usesCurrentSelection);
    assert(unselected.activeUuids == std::vector<std::string>{"new"});
    const SceneElementKind kind =
        target == DragTarget::Fixtures   ? SceneElementKind::Fixture
        : target == DragTarget::Trusses  ? SceneElementKind::Truss
        : target == DragTarget::Supports ? SceneElementKind::Support
                                         : SceneElementKind::SceneObject;
    assert(Bucket(unselected.selection, kind) ==
           std::vector<std::string>{"new"});
  }
  return 0;
}
