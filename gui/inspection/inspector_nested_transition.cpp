#include "inspection/inspector_nested_transition.h"

#include <utility>

namespace gui::inspection {

// Begins or supersedes a pending nested transition with a retained parent.
void InspectorNestedTransition::Begin(
    std::shared_ptr<const DisplayedPackageContext> parent,
    std::uint64_t sourceGeneration) {
  parent_ = std::move(parent);
  sourceGeneration_ = sourceGeneration;
  workerGeneration_ = 0;
}

// Associates the independently generated worker request with this transition.
void InspectorNestedTransition::SetWorkerGeneration(
    std::uint64_t workerGeneration) {
  workerGeneration_ = workerGeneration;
}

// Reports whether a source result belongs to the pending transition.
bool InspectorNestedTransition::Matches(
    std::uint64_t sourceGeneration) const {
  return parent_ && sourceGeneration == sourceGeneration_;
}

// Reports whether a worker error belongs to the pending transition.
bool InspectorNestedTransition::MatchesWorker(
    std::uint64_t workerGeneration) const {
  return parent_ && workerGeneration == workerGeneration_;
}

// Reports whether an immutable parent is retained for pending navigation.
bool InspectorNestedTransition::IsPending() const {
  return static_cast<bool>(parent_);
}

// Commits the transition by transferring its retained parent to the caller.
std::shared_ptr<const DisplayedPackageContext>
InspectorNestedTransition::Commit() {
  auto parent = std::move(parent_);
  sourceGeneration_ = 0;
  workerGeneration_ = 0;
  return parent;
}

// Cancels a transition without changing the retained displayed context.
void InspectorNestedTransition::Cancel() {
  parent_.reset();
  sourceGeneration_ = 0;
  workerGeneration_ = 0;
}

} // namespace gui::inspection
