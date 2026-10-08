#include "windows/symbol_preview_exporter.h"

#include "symbols/Symbol2DSvg.h"

#include <fstream>

namespace symbol_preview {
bool ExportSymbolToSvg(const symbols::Symbol2D &symbol,
                       const std::string &filePath,
                       std::string &errorMessage) {
  std::string svgContent;
  if (!symbols::SerializeSymbolToSvg(symbol, svgContent, errorMessage))
    return false;

  std::ofstream file(filePath);
  if (!file.is_open()) {
    errorMessage = "Could not open the output file.";
    return false;
  }

  file << svgContent;
  return file.good();
}

bool ExportSymbolToSvgString(const symbols::Symbol2D &symbol,
                             std::string &svgContent,
                             std::string &errorMessage) {
  return symbols::SerializeSymbolToSvg(symbol, svgContent, errorMessage);
}

} // namespace symbol_preview
