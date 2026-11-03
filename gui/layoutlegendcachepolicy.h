#pragma once

#include "layouts/LayoutCollection.h"

namespace gui {
// Check every existing raster against its own semantic identity before the
// selected-legend summary can short-circuit a refresh. Keep valid symbols and
// persistent rasters; the renderer already validates their content identities.
template <typename CacheMap, typename ContentHash>
bool InvalidateChangedLegendRasters(
    const std::vector<layouts::LayoutLegendDefinition> &legends,
    CacheMap &caches, const ContentHash &contentHash) {
  bool changed = false;
  for (const auto &legend : legends) {
    const auto it = caches.find(legend.id);
    if (it != caches.end() && it->second.contentHash != contentHash(legend)) {
      it->second.renderDirty = true;
      changed = true;
    }
  }
  return changed;
}
} // namespace gui
