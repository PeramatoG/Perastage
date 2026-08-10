#include "symbols/fixture_symbol_preparation_coordinator.h"

#include <cassert>
#include <string>

// Verifies exact-mode coalescing, cooperative progress, priority, and epochs.
int main() {
  symbols::FixtureSymbolPreparationCoordinator coordinator;
  const auto epoch = coordinator.BeginProjectEpoch();
  const symbols::FixtureSymbolPreparationKey standard{"/physical/unit.gdtf",
                                                       "Standard"};
  const symbols::FixtureSymbolPreparationKey extended{"/physical/unit.gdtf",
                                                       "Extended"};
  for (int fixture = 0; fixture < 100; ++fixture)
    assert(coordinator.Request(standard, "Fixture@Perastage.gdtf",
                               symbols::FixtureSymbolPreparationPriority::Automatic) ==
           (fixture == 0));
  assert(coordinator.Request(extended, "Fixture@Perastage.gdtf",
                             symbols::FixtureSymbolPreparationPriority::Automatic));
  assert(coordinator.PendingCount() == 2);

  assert(coordinator.Cancel(standard, epoch));
  assert(coordinator.Request(
      standard, "Fixture@Perastage.gdtf",
      symbols::FixtureSymbolPreparationPriority::Automatic));
  assert(coordinator.PendingCount() == 2);

  assert(!coordinator.Request(standard, "Fixture@Perastage.gdtf",
                              symbols::FixtureSymbolPreparationPriority::Manual));
  assert(coordinator.NextQueued()->key == standard);
  assert(coordinator.BeginCapture(standard, epoch));
  assert(coordinator.CompleteCapture(standard, epoch));
  assert(coordinator.Find(standard)->state ==
         symbols::FixtureSymbolPreparationState::Processing);
  assert(coordinator.BeginPublishing(standard, epoch));

  assert(coordinator.BeginCapture(extended, epoch));
  assert(coordinator.CompleteCapture(extended, epoch));
  assert(!coordinator.BeginPublishing(extended, epoch));
  assert(coordinator.Complete(standard, epoch, true));
  assert(coordinator.BeginPublishing(extended, epoch));

  const auto replacementEpoch = coordinator.BeginProjectEpoch();
  assert(replacementEpoch != epoch);
  assert(!coordinator.Complete(extended, epoch, true));
  assert(coordinator.Find(extended)->state ==
         symbols::FixtureSymbolPreparationState::Stale);
  return 0;
}
