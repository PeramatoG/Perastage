#include <cassert>
#include <string>

#include "../gui/windows/symbol_fixture_apply_presentation.h"

int main() {
  symbol_preview::ApplySymbolsResult saved;
  saved.success = true;
  saved.projectSymbolsUpdated = true;
  const auto savedMessage = symbol_preview::BuildApplySymbolsPresentation(saved);
  assert(savedMessage.kind == symbol_preview::ApplySymbolsMessageKind::Success);
  assert(savedMessage.message.find("applied to the project") != std::string::npos);
  assert(savedMessage.message.find("persist the symbol override") != std::string::npos);
  assert(savedMessage.message.find("GDTF") == std::string::npos);

  auto unsaved = saved;
  unsaved.unsavedProject = true;
  const auto unsavedMessage = symbol_preview::BuildApplySymbolsPresentation(unsaved);
  assert(unsavedMessage.kind == symbol_preview::ApplySymbolsMessageKind::Success);
  assert(unsavedMessage.message.find("unsaved project") != std::string::npos);
  assert(unsavedMessage.message.find("Save the project") != std::string::npos);
  assert(unsavedMessage.message.find("library") == std::string::npos);

  symbol_preview::ApplySymbolsResult failure;
  failure.diagnostic = "The selected fixture no longer exists.";
  const auto failureMessage = symbol_preview::BuildApplySymbolsPresentation(failure);
  assert(failureMessage.kind == symbol_preview::ApplySymbolsMessageKind::Error);
  assert(failureMessage.message == failure.diagnostic);

  auto noUpdate = saved;
  noUpdate.projectSymbolsUpdated = false;
  const auto noUpdateMessage = symbol_preview::BuildApplySymbolsPresentation(noUpdate);
  assert(noUpdateMessage.kind == symbol_preview::ApplySymbolsMessageKind::Error);
  assert(!noUpdateMessage.message.empty());
  return 0;
}
