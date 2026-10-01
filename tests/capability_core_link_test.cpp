#include "capability/capability_catalog.h"
#include "command/command_operation_ids.h"

#include <cassert>

// Verifies that capability discovery links without Command execution targets.
int main() {
  const auto *descriptor = perastage::capability::Find(
      perastage::command::selection::kClearCommandId);
  assert(descriptor != nullptr);
  assert(perastage::capability::ValidateRequestShape(
             {perastage::command::selection::kClearCommandId, {}})
             .empty());
  return 0;
}
