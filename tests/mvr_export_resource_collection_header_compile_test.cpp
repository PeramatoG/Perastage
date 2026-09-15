#include "mvr_export_resource_collection.h"

#include <type_traits>

// Verifies that the internal resource plan remains independently consumable.
int main() {
  static_assert(std::is_move_constructible_v<mvr_export_resources::ResourcePlan>);
  mvr_export_resources::ResourceEntry entry;
  entry.kind = mvr_export_resources::ResourceKind::Gdtf;
  entry.provenance =
      mvr_export_resources::ResourceProvenance::CompatibilityFallback;
  return entry.archivePath.empty() ? 0 : 1;
}
