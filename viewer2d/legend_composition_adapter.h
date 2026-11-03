#pragma once

#include "layouts/LegendComposition.h"
#include "layouts/LegendContent.h"
#include "symbols/PerastageSvgSymbol.h"
#include "symbolcache.h"

namespace viewer2d {

struct ResolvedLegendSymbol {
  const PerastageSvgSymbolData *svg = nullptr;
  const SymbolDefinition *commands = nullptr;
};
struct LegendCompositionResources {
  layouts::composition::LegendCompositionInput input;
  std::vector<ResolvedLegendSymbol> symbols;
};
using LegendSvgResolver = std::function<const PerastageSvgSymbolData *(
    const std::string &, const std::string &, SymbolViewKind)>;

// Bottom retains the legacy preferred-view fallback. Front and Side require
// exact command snapshots. The SVG resolver retains its existing GDTF/project
// policy, and Side requests the existing internal Right representation.
LegendCompositionResources ResolveLegendComposition(
    const std::vector<layouts::LegendResolvedItem> &items,
    const SymbolDefinitionSnapshot *snapshot, const LegendSvgResolver &resolveSvg);

} // namespace viewer2d
