#pragma once

#include "Symbol2D.h"

#include <string>

namespace symbols {

// Serializes fixture-level vectors without capture, UI, or filesystem policy.
// Hole contours use closed absolute M/L/Z subpaths with even-odd filling.
bool SerializeSymbolToSvg(const Symbol2D &symbol, std::string &svgContent,
                          std::string &errorMessage);

} // namespace symbols
