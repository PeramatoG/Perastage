#include "services/fixture_symbol_preparation_notifications.h"
#include "symbols/fixture_symbol_operation_counters.h"

#include <cassert>
#include <memory>
#include <thread>
#include <wx/init.h>

// Exercise the actual wx mailbox without rendering or a live window.
int main() {
  wxInitializer wx;
  assert(wx.IsOk());
  int requests = 0;
  int completions = 0;
  auto mailbox = std::make_shared<gui::FixtureSymbolPreparationNotifications>(
      [&](const auto &) { ++requests; }, [&] { ++completions; });
  const symbols::FixtureSymbolPreparationKey key{"/physical.gdtf", "Exact Mode"};
  std::thread renderer([&] {
    for (int i = 0; i < 100; ++i)
      mailbox->Request(key);
  });
  renderer.join();
  assert(!requests && !completions);
  mailbox->ProcessPendingEvents();
  assert(requests == 1 && completions == 0);
  // A blocked worker produces no completion events or wakeups.
  std::thread worker([&] { mailbox->Completed(); });
  worker.join();
  mailbox->ProcessPendingEvents();
  assert(requests == 1 && completions == 1);
  mailbox->Request({key.effectiveGdtfPath, "Other Mode"});
  mailbox->ProcessPendingEvents();
  assert(requests == 2);
  mailbox->Completed();
  mailbox->Request(key);
  mailbox->Close();
  // An in-flight poster can retain the destination after service destruction.
  std::thread delayed([mailbox] {
    mailbox->Completed();
    mailbox->Request({"/stale.gdtf", "Exact Mode"});
  });
  delayed.join();
  assert(requests == 2 && completions == 1);
  return 0;
}
