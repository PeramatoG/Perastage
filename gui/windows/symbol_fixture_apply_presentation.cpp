#include "windows/symbol_fixture_apply_presentation.h"

namespace symbol_preview {

// Formats fixture-symbol persistence outcomes without overstating archive ownership.
ApplySymbolsPresentation BuildApplySymbolsPresentation(
    const ApplySymbolsResult &result) {
  ApplySymbolsPresentation presentation;
  if (!result.success || !result.sceneUpdated) {
    presentation.kind = ApplySymbolsMessageKind::Error;
    presentation.message = result.diagnostic.empty()
                               ? "The project fixture GDTF was not updated."
                               : result.diagnostic;
    if (result.libraryUpdated)
      presentation.message += " The library-only update was not project persistence.";
    return presentation;
  }

  if (result.libraryUpdated) {
    presentation.kind = ApplySymbolsMessageKind::Success;
    presentation.message =
        "Symbol views were applied to the project fixture GDTF and the fixture library derivative.";
    return presentation;
  }

  presentation.kind = result.warnings.empty() ? ApplySymbolsMessageKind::Success
                                               : ApplySymbolsMessageKind::Warning;
  presentation.message = "Symbol views were applied to the project fixture GDTF.";
  if (!result.warnings.empty()) {
    presentation.message += " Fixture library synchronization failed: ";
    for (std::size_t i = 0; i < result.warnings.size(); ++i) {
      if (i > 0)
        presentation.message += " | ";
      presentation.message += result.warnings[i];
    }
  }
  return presentation;
}

} // namespace symbol_preview
