#include "symbols/fixture_symbol_operation_counters.h"
#include "services/fixture_symbol_preparation_notifications.h"

#include <utility>

namespace gui {

FixtureSymbolPreparationNotifications::FixtureSymbolPreparationNotifications(
    RequestHandler request, std::function<void()> completed)
    : request_(std::move(request)), completed_(std::move(completed)) {
  Bind(wxEVT_THREAD, &FixtureSymbolPreparationNotifications::Dispatch, this);
}

void FixtureSymbolPreparationNotifications::ScheduleLocked() {
  if (scheduled_)
    return;
  scheduled_ = true;
  symbols::diagnostics::Count(symbols::diagnostics::Operation::GuiRequestPost);
  wxQueueEvent(this, new wxThreadEvent(wxEVT_THREAD));
}

void FixtureSymbolPreparationNotifications::Request(const Key &key) {
  std::lock_guard lock(mutex_);
  if (closed_)
    return;
  requests_.insert(key);
  ScheduleLocked();
}

void FixtureSymbolPreparationNotifications::Completed() {
  std::lock_guard lock(mutex_);
  if (closed_)
    return;
  completionPending_ = true;
  ScheduleLocked();
}

void FixtureSymbolPreparationNotifications::Dispatch(wxThreadEvent &) {
  decltype(requests_) requests;
  bool completed;
  {
    std::lock_guard lock(mutex_);
    scheduled_ = false;
    if (closed_)
      return;
    // Bound each GUI delivery even when a large batch arrives off-thread.
    for (unsigned count = 0; count < 64 && !requests_.empty(); ++count)
      requests.insert(requests_.extract(requests_.begin()));
    if (!requests_.empty())
      ScheduleLocked();
    completed = std::exchange(completionPending_, false);
  }
  // The service drains requests in bounded idle slices, independently of this
  // event. Dispatch never performs capture or processing.
  for (const auto &key : requests)
    request_(key);
  if (completed)
    completed_();
}

void FixtureSymbolPreparationNotifications::Close() {
  {
    std::lock_guard lock(mutex_);
    closed_ = true;
    requests_.clear();
    request_ = {};
    completed_ = {};
  }
  DeletePendingEvents();
}

} // namespace gui
