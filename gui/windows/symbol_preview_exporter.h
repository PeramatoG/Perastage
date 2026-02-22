#pragma once

#include <string>

#include "symbols/Symbol2D.h"

namespace symbol_preview {

bool ExportSymbolToSvg(const symbols::Symbol2D &symbol,
                       const std::string &filePath,
                       std::string &errorMessage);

bool ExportSymbolToSvgString(const symbols::Symbol2D &symbol,
                             std::string &svgContent,
                             std::string &errorMessage);

} // namespace symbol_preview
