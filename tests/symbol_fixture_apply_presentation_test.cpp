#include <cassert>
#include <string>

#include "../gui/windows/symbol_fixture_apply_presentation.h"

// Verifies that apply-result messages preserve project and library ownership semantics.
int main() {
  symbol_preview::ApplySymbolsResult complete;
  complete.success = true;
  complete.sceneUpdated = true;
  complete.libraryUpdated = true;
  const auto completeMessage =
      symbol_preview::BuildApplySymbolsPresentation(complete);
  assert(completeMessage.kind == symbol_preview::ApplySymbolsMessageKind::Success);
  assert(completeMessage.message.find("project fixture GDTF") != std::string::npos);
  assert(completeMessage.message.find("library derivative") != std::string::npos);

  symbol_preview::ApplySymbolsResult warning = complete;
  warning.libraryUpdated = false;
  warning.warnings = {"dictionary storage is unavailable"};
  const auto warningMessage =
      symbol_preview::BuildApplySymbolsPresentation(warning);
  assert(warningMessage.kind == symbol_preview::ApplySymbolsMessageKind::Warning);
  assert(warningMessage.message.find("project fixture GDTF") != std::string::npos);
  assert(warningMessage.message.find("synchronization failed") != std::string::npos);
  assert(warningMessage.message.find("and the fixture library derivative") ==
         std::string::npos);

  symbol_preview::ApplySymbolsResult failure;
  failure.diagnostic = "scene replacement failed";
  const auto failureMessage =
      symbol_preview::BuildApplySymbolsPresentation(failure);
  assert(failureMessage.kind == symbol_preview::ApplySymbolsMessageKind::Error);
  assert(failureMessage.message == failure.diagnostic);

  symbol_preview::ApplySymbolsResult libraryOnly;
  libraryOnly.success = true;
  libraryOnly.libraryUpdated = true;
  const auto libraryOnlyMessage =
      symbol_preview::BuildApplySymbolsPresentation(libraryOnly);
  assert(libraryOnlyMessage.kind == symbol_preview::ApplySymbolsMessageKind::Error);
  assert(libraryOnlyMessage.message.find("not project persistence") !=
         std::string::npos);

  const auto fallbackMessage =
      symbol_preview::BuildApplySymbolsPresentation({});
  assert(fallbackMessage.kind == symbol_preview::ApplySymbolsMessageKind::Error);
  assert(!fallbackMessage.message.empty());
  return 0;
}
