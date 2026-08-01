#pragma once

#include <string>

#include "windows/symbol_fixture_applier.h"

namespace symbol_preview {

enum class ApplySymbolsMessageKind { Success, Warning, Error };

struct ApplySymbolsPresentation {
  ApplySymbolsMessageKind kind = ApplySymbolsMessageKind::Error;
  std::string message;
};

// Formats fixture-symbol persistence outcomes without depending on GUI controls.
ApplySymbolsPresentation BuildApplySymbolsPresentation(
    const ApplySymbolsResult &result);

} // namespace symbol_preview
