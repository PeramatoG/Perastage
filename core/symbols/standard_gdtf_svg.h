#pragma once

#include "Symbol2D.h"
#include "symbol_view_kind.h"

#include <string>

namespace symbols {

struct StandardGdtfSvgCandidate {
  SymbolViewKind viewKind = SymbolViewKind::Top;
  std::string svg;
  double offsetXmm = 0.0;
  double offsetYmm = 0.0;
};

// GDTF 1.2 output is distinct from the project-symbol persistence target.
// Geometry is expressed in millimeters; alignment belongs to Model attributes.
bool SerializeStandardGdtfSvg(const Symbol2D &symbol, SymbolViewKind view,
                             StandardGdtfSvgCandidate &candidate,
                             std::string &errorMessage);

std::string StandardGdtfSvgPath(const std::string &modelFile,
                                SymbolViewKind view);

} // namespace symbols
