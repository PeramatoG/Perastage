#pragma once

#include "LayoutCollection.h"

namespace layouts {

// Resolved scene-wide aggregation. Resources remain owned by the caller.
struct LegendSourceItem {
  std::string typeName;
  int count = 0;
  std::optional<int> channelCount;
  std::string symbolKey;
  std::string persistentSymbolIdentity;
  std::string gdtfPath;
  std::optional<std::string> symbolFillHex;
};
struct LegendResolvedItem : LegendSourceItem {
  std::string displayName;
  bool showBottomSymbol = true;
  bool showFrontSymbol = true;
  bool showSideSymbol = false;
};

std::vector<LegendResolvedItem> ResolveLegendItems(
    const std::vector<LegendSourceItem> &source,
    const LayoutLegendDefinition *legend);

} // namespace layouts
