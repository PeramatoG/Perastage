#include "windows/symbol_fixture_apply_presentation.h"

namespace symbol_preview {

ApplySymbolsPresentation BuildApplySymbolsPresentation(
    const ApplySymbolsResult &result) {
  ApplySymbolsPresentation presentation;
  if (!result.success || !result.projectSymbolsUpdated) {
    presentation.message = result.diagnostic.empty()
        ? "The project fixture symbol was not updated." : result.diagnostic;
    return presentation;
  }
  presentation.kind = ApplySymbolsMessageKind::Success;
  presentation.message = result.unsavedProject
      ? "Symbol views were applied to the unsaved project. Save the project to persist the symbol override."
      : "Symbol views were applied to the project. Save the project to persist the symbol override.";
  return presentation;
}

} // namespace symbol_preview
