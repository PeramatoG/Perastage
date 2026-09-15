#include "selection_interaction_policy.h"

#include <algorithm>
#include <set>

namespace viewer3d::interaction {
namespace {

// Reports whether every clicked UUID already belongs to the selection.
bool ContainsAllUuids(const std::vector<std::string> &selection,
                      const std::vector<std::string> &clickedUuids) {
  return std::all_of(clickedUuids.begin(), clickedUuids.end(),
                     [&](const std::string &uuid) {
                       return std::find(selection.begin(), selection.end(),
                                        uuid) != selection.end();
                     });
}

// Adds or removes all clicked UUIDs as one grouped toggle operation.
std::vector<std::string>
ToggleSelectionUuids(std::vector<std::string> selection,
                     const std::vector<std::string> &clickedUuids) {
  if (clickedUuids.empty())
    return selection;
  const bool removeClickedUuids = ContainsAllUuids(selection, clickedUuids);
  for (const auto &uuid : clickedUuids) {
    const auto it = std::find(selection.begin(), selection.end(), uuid);
    if (removeClickedUuids) {
      if (it != selection.end())
        selection.erase(it);
    } else if (it == selection.end()) {
      selection.push_back(uuid);
    }
  }
  return selection;
}

} // namespace

// Adds missing UUIDs to a selection while preserving its existing order.
std::vector<std::string>
AddSelectionUuids(std::vector<std::string> selection,
                  const std::vector<std::string> &addedUuids) {
  for (const auto &uuid : addedUuids) {
    if (std::find(selection.begin(), selection.end(), uuid) ==
        selection.end()) {
      selection.push_back(uuid);
    }
  }
  return selection;
}

// Applies replace, additive, or grouped-toggle semantics to one typed
// selection.
std::vector<std::string>
ResolveClickedSelection(const std::vector<std::string> &currentSelection,
                        const std::vector<std::string> &clickedUuids,
                        bool additive, bool addOnly) {
  if (!additive)
    return clickedUuids;
  if (addOnly)
    return AddSelectionUuids(currentSelection, clickedUuids);
  return ToggleSelectionUuids(currentSelection, clickedUuids);
}

// Flattens typed selections into a stable, deduplicated viewer UUID list.
std::vector<std::string>
FlattenTypedSelection(const TypedSelection &selection) {
  std::set<std::string> mergedSelection;
  mergedSelection.insert(selection.fixtures.begin(), selection.fixtures.end());
  mergedSelection.insert(selection.trusses.begin(), selection.trusses.end());
  mergedSelection.insert(selection.supports.begin(), selection.supports.end());
  mergedSelection.insert(selection.sceneObjects.begin(),
                         selection.sceneObjects.end());
  return {mergedSelection.begin(), mergedSelection.end()};
}

} // namespace viewer3d::interaction
