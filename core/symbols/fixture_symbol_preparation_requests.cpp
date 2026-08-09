#include "symbols/fixture_symbol_preparation_requests.h"

#include <mutex>
#include <utility>

namespace symbols {
namespace {
std::mutex handlerMutex;
FixtureSymbolPreparationRequest requestHandler;
} // namespace

// Replaces the process-local, non-persistent preparation request handler.
void SetFixtureSymbolPreparationRequestHandler(
    FixtureSymbolPreparationRequest handler) {
  std::lock_guard<std::mutex> lock(handlerMutex);
  requestHandler = std::move(handler);
}

// Delivers a fire-and-forget missing-symbol request without owning GUI policy.
void RequestFixtureSymbolPreparation(const std::string &effectiveGdtfPath,
                                     const std::string &exactGdtfMode) {
  FixtureSymbolPreparationRequest handler;
  {
    std::lock_guard<std::mutex> lock(handlerMutex);
    handler = requestHandler;
  }
  if (handler)
    handler(effectiveGdtfPath, exactGdtfMode);
}

} // namespace symbols
