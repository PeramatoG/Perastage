#include "pdf/layout_pdf_symbol_usage.h"

#include <cassert>

namespace {

// Verifies fixture commands cannot collapse into a labels-only zero/zero
// report.
void TestFallbackUsageBeforePreparation() {
  CommandBuffer buffer;
  buffer.commands.emplace_back(TextCommand{});
  buffer.commands.emplace_back(PlaceSymbolCommand{"fixture", {}});
  buffer.commands.emplace_back(SymbolInstanceCommand{42, {}});
  const auto usage =
      layout_pdf_internal::CountLayoutPdfSymbolUsage(buffer, nullptr);
  assert(usage.perastageSymbolInstances == 0);
  assert(usage.fallbackSymbolInstances == 2);
}

// Verifies a published stored SVG is reported through the same PDF view path.
void TestStoredSvgUsageAfterPublication() {
  CommandBuffer buffer;
  buffer.commands.emplace_back(SymbolInstanceCommand{7, {}});
  SymbolDefinition definition;
  definition.symbolId = 7;
  definition.source = SymbolDefinition::Source::PerastageSvg;
  SymbolDefinitionSnapshot symbols;
  symbols.emplace(7, definition);
  const auto usage =
      layout_pdf_internal::CountLayoutPdfSymbolUsage(buffer, &symbols);
  assert(usage.perastageSymbolInstances == 1);
  assert(usage.fallbackSymbolInstances == 0);
}

} // namespace

// Runs layout PDF symbol-usage regressions before and after preparation.
int main() {
  TestFallbackUsageBeforePreparation();
  TestStoredSvgUsageAfterPublication();
  return 0;
}
