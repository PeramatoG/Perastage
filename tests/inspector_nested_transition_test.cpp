#include "inspection/inspector_nested_transition.h"

#include <cassert>
#include <memory>

namespace {

// Creates a retained context used to identify the committed parent source.
std::shared_ptr<const gui::inspection::DisplayedPackageContext>
Context(const char *fingerprint) {
  auto context = std::make_shared<gui::inspection::DisplayedPackageContext>();
  context->fingerprint = fingerprint;
  return context;
}

} // namespace

// Verifies commit, failure, and latest-request-wins transition ownership.
int main() {
  gui::inspection::InspectorNestedTransition transition;
  const auto firstParent = Context("first-parent");
  const auto latestParent = Context("latest-parent");

  transition.Begin(firstParent, 10);
  transition.SetWorkerGeneration(20);
  assert(transition.IsPending());
  assert(transition.Matches(10));
  assert(transition.MatchesWorker(20));
  assert(!transition.Matches(11));

  transition.Begin(latestParent, 11);
  transition.SetWorkerGeneration(21);
  assert(!transition.Matches(10));
  assert(!transition.MatchesWorker(20));
  assert(transition.Matches(11));
  assert(transition.Commit() == latestParent);
  assert(!transition.IsPending());

  transition.Begin(firstParent, 12);
  transition.Cancel();
  assert(!transition.IsPending());
  assert(!transition.Matches(12));
  return 0;
}
