#include "symbols/fixture_symbol_preparation_coordinator.h"

#include <functional>

namespace symbols {

// Hashes the physical resource and exact mode without normalizing either value.
std::size_t FixtureSymbolPreparationKeyHash::operator()(
    const FixtureSymbolPreparationKey &key) const {
  const std::size_t resource = std::hash<std::string>{}(key.effectiveGdtfPath);
  const std::size_t mode = std::hash<std::string>{}(key.exactGdtfMode);
  return resource ^ (mode + 0x9e3779b9U + (resource << 6U) + (resource >> 2U));
}

// Starts a new runtime project lifetime and makes all previous work stale.
std::uint64_t FixtureSymbolPreparationCoordinator::BeginProjectEpoch() {
  ++epoch_;
  for (auto &[key, job] : jobs_) {
    (void)key;
    if (job.state != FixtureSymbolPreparationState::Completed &&
        job.state != FixtureSymbolPreparationState::Failed)
      job.state = FixtureSymbolPreparationState::Stale;
  }
  publishingTargets_.clear();
  return epoch_;
}

// Coalesces a request and promotes an automatic request when manual work
// arrives.
bool FixtureSymbolPreparationCoordinator::Request(
    const FixtureSymbolPreparationKey &key, std::string canonicalTarget,
    FixtureSymbolPreparationPriority priority) {
  auto existing = jobs_.find(key);
  if (existing != jobs_.end() && existing->second.epoch == epoch_) {
    if (existing->second.state == FixtureSymbolPreparationState::Cancelled) {
      existing->second = {key, std::move(canonicalTarget), epoch_, priority};
      return true;
    }
    if (priority == FixtureSymbolPreparationPriority::Manual)
      existing->second.priority = priority;
    return false;
  }
  jobs_[key] = {key, std::move(canonicalTarget), epoch_, priority};
  return true;
}

// Selects the next queued job, preferring explicit manual requests.
std::optional<FixtureSymbolPreparationJob>
FixtureSymbolPreparationCoordinator::NextQueued() {
  FixtureSymbolPreparationJob *selected = nullptr;
  for (auto &[key, job] : jobs_) {
    (void)key;
    if (job.epoch != epoch_ ||
        job.state != FixtureSymbolPreparationState::Queued)
      continue;
    if (!selected || job.priority == FixtureSymbolPreparationPriority::Manual)
      selected = &job;
    if (selected->priority == FixtureSymbolPreparationPriority::Manual)
      break;
  }
  return selected ? std::optional<FixtureSymbolPreparationJob>(*selected)
                  : std::nullopt;
}

// Starts cooperative capture only for work owned by the active epoch.
bool FixtureSymbolPreparationCoordinator::BeginCapture(
    const FixtureSymbolPreparationKey &key, std::uint64_t epoch) {
  auto it = jobs_.find(key);
  if (!IsCurrent(epoch) || it == jobs_.end() || it->second.epoch != epoch ||
      it->second.state != FixtureSymbolPreparationState::Queued)
    return false;
  it->second.state = FixtureSymbolPreparationState::Capturing;
  return true;
}

// Completes one atomic GUI-thread four-view capture.
bool FixtureSymbolPreparationCoordinator::CompleteCapture(
    const FixtureSymbolPreparationKey &key, std::uint64_t epoch) {
  auto it = jobs_.find(key);
  if (!IsCurrent(epoch) || it == jobs_.end() ||
      it->second.state != FixtureSymbolPreparationState::Capturing)
    return false;
  it->second.state = FixtureSymbolPreparationState::Processing;
  return true;
}

// Serializes publication for jobs whose exact modes share a canonical target.
bool FixtureSymbolPreparationCoordinator::BeginPublishing(
    const FixtureSymbolPreparationKey &key, std::uint64_t epoch) {
  auto it = jobs_.find(key);
  if (!IsCurrent(epoch) || it == jobs_.end() ||
      it->second.state != FixtureSymbolPreparationState::Processing ||
      publishingTargets_.contains(it->second.canonicalTarget))
    return false;
  publishingTargets_[it->second.canonicalTarget] = key;
  it->second.state = FixtureSymbolPreparationState::Publishing;
  return true;
}

// Finishes current work and releases its canonical publication target.
bool FixtureSymbolPreparationCoordinator::Complete(
    const FixtureSymbolPreparationKey &key, std::uint64_t epoch, bool success) {
  auto it = jobs_.find(key);
  if (!IsCurrent(epoch) || it == jobs_.end() ||
      it->second.state != FixtureSymbolPreparationState::Publishing)
    return false;
  publishingTargets_.erase(it->second.canonicalTarget);
  it->second.state = success ? FixtureSymbolPreparationState::Completed
                             : FixtureSymbolPreparationState::Failed;
  return true;
}

// Stops active work in a failed cooldown state for the current project epoch.
bool FixtureSymbolPreparationCoordinator::Fail(
    const FixtureSymbolPreparationKey &key, std::uint64_t epoch) {
  auto it = jobs_.find(key);
  if (!IsCurrent(epoch) || it == jobs_.end() || it->second.epoch != epoch)
    return false;
  publishingTargets_.erase(it->second.canonicalTarget);
  it->second.state = FixtureSymbolPreparationState::Failed;
  return true;
}

// Cancels work without suppressing a later automatic request in the same epoch.
bool FixtureSymbolPreparationCoordinator::Cancel(
    const FixtureSymbolPreparationKey &key, std::uint64_t epoch) {
  auto it = jobs_.find(key);
  if (!IsCurrent(epoch) || it == jobs_.end() || it->second.epoch != epoch)
    return false;
  publishingTargets_.erase(it->second.canonicalTarget);
  it->second.state = FixtureSymbolPreparationState::Cancelled;
  return true;
}

// Completes queued work when a serialized predecessor already published
// symbols.
bool FixtureSymbolPreparationCoordinator::Skip(
    const FixtureSymbolPreparationKey &key, std::uint64_t epoch) {
  auto it = jobs_.find(key);
  if (!IsCurrent(epoch) || it == jobs_.end() || it->second.epoch != epoch ||
      it->second.state != FixtureSymbolPreparationState::Queued)
    return false;
  it->second.state = FixtureSymbolPreparationState::Completed;
  return true;
}

// Reports whether a callback still belongs to the current project lifetime.
bool FixtureSymbolPreparationCoordinator::IsCurrent(std::uint64_t epoch) const {
  return epoch == epoch_;
}

// Counts aggregate active work for non-modal status display.
std::size_t FixtureSymbolPreparationCoordinator::PendingCount() const {
  std::size_t count = 0;
  for (const auto &[key, job] : jobs_) {
    (void)key;
    if (job.epoch == epoch_ &&
        job.state != FixtureSymbolPreparationState::Completed &&
        job.state != FixtureSymbolPreparationState::Failed &&
        job.state != FixtureSymbolPreparationState::Cancelled &&
        job.state != FixtureSymbolPreparationState::Stale)
      ++count;
  }
  return count;
}

// Finds runtime state for diagnostics and controlled scheduling.
const FixtureSymbolPreparationJob *FixtureSymbolPreparationCoordinator::Find(
    const FixtureSymbolPreparationKey &key) const {
  const auto it = jobs_.find(key);
  return it == jobs_.end() ? nullptr : &it->second;
}

} // namespace symbols
