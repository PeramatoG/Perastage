#pragma once

#include <string>
#include <vector>

namespace viewer3d::interaction {

struct TypedSelection {
  std::vector<std::string> fixtures;
  std::vector<std::string> trusses;
  std::vector<std::string> supports;
  std::vector<std::string> sceneObjects;
};

// Adds missing UUIDs to a selection while preserving its existing order.
std::vector<std::string>
AddSelectionUuids(std::vector<std::string> selection,
                  const std::vector<std::string> &addedUuids);

// Applies replace, additive, or grouped-toggle semantics to one typed
// selection.
std::vector<std::string>
ResolveClickedSelection(const std::vector<std::string> &currentSelection,
                        const std::vector<std::string> &clickedUuids,
                        bool additive, bool addOnly);

// Flattens typed selections into a stable, deduplicated viewer UUID list.
std::vector<std::string> FlattenTypedSelection(const TypedSelection &selection);

} // namespace viewer3d::interaction
