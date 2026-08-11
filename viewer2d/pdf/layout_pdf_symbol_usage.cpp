#include "layout_pdf_symbol_usage.h"

#include <variant>

namespace layout_pdf_internal {

// Counts stored-SVG and fallback instances represented by one layout view
// buffer.
LayoutPdfSymbolUsage
CountLayoutPdfSymbolUsage(const CommandBuffer &buffer,
                          const SymbolDefinitionSnapshot *symbols) {
  LayoutPdfSymbolUsage usage;
  for (const auto &command : buffer.commands) {
    if (std::holds_alternative<PlaceSymbolCommand>(command)) {
      ++usage.fallbackSymbolInstances;
      continue;
    }
    const auto *instance = std::get_if<SymbolInstanceCommand>(&command);
    if (!instance)
      continue;
    const SymbolDefinition *definition = nullptr;
    if (symbols) {
      const auto definitionIt = symbols->find(instance->symbolId);
      if (definitionIt != symbols->end())
        definition = &definitionIt->second;
    }
    if (definition &&
        definition->source == SymbolDefinition::Source::PerastageSvg)
      ++usage.perastageSymbolInstances;
    else
      ++usage.fallbackSymbolInstances;
  }
  return usage;
}

} // namespace layout_pdf_internal
