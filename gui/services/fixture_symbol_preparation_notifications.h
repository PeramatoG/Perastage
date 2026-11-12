#pragma once

#include <functional>
#include <mutex>
#include <unordered_set>
#include <wx/event.h>

#include "symbols/fixture_symbol_preparation_coordinator.h"

namespace gui {

// A shared event destination keeps posts safe across service/window teardown.
// Only the GUI thread dispatches or closes this mailbox.
class FixtureSymbolPreparationNotifications : public wxEvtHandler {
public:
  using Key = symbols::FixtureSymbolPreparationKey;
  using RequestHandler = std::function<void(const Key &)>;
  FixtureSymbolPreparationNotifications(RequestHandler request,
                                        std::function<void()> completed);
  void Request(const Key &key);
  void Completed();
  void Close();

private:
  void ScheduleLocked();
  void Dispatch(wxThreadEvent &);
  std::mutex mutex_;
  std::unordered_set<Key, symbols::FixtureSymbolPreparationKeyHash> requests_;
  RequestHandler request_;
  std::function<void()> completed_;
  bool completionPending_ = false;
  bool scheduled_ = false;
  bool closed_ = false;
};

} // namespace gui
