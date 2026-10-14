#pragma once

#include <chrono>
#include <limits>
#include <string>
#include <unordered_set>

namespace viewer3d::resources {
// String identities only: queued work never retains scene-container pointers.
// A new generation cancels unfinished work, including same-base replacements.
class ResourceWorkBudget {
public:
  bool Begin(size_t generation, size_t limit,
             std::chrono::milliseconds duration) {
    const bool changed = !hasGeneration || generation != currentGeneration;
    if (!hasGeneration || generation != currentGeneration) {
      completed.clear();
      currentGeneration = generation;
      hasGeneration = true;
    }
    granted.clear();
    pending = false;
    maximum = limit;
    deadline = duration.count() > 0 ? Clock::now() + duration : Clock::time_point::max();
    return changed;
  }
  bool Acquire(const std::string &identity) {
    if (granted.contains(identity)) return true;
    if (completed.contains(identity)) return false;
    // Always allow one indivisible resource, even if traversal used the budget.
    if (granted.size() >= maximum ||
        (!granted.empty() && Clock::now() >= deadline)) {
      pending = true;
      return false;
    }
    granted.insert(identity);
    return true;
  }
  void Finish() { completed.insert(granted.begin(), granted.end()); }
  void Cancel() { completed.clear(); granted.clear(); hasGeneration = false; pending = false; }
  bool HasPending() const { return pending; }
  size_t Processed() const { return granted.size(); }
  size_t Completed() const { return completed.size(); }
private:
  using Clock = std::chrono::steady_clock;
  std::unordered_set<std::string> completed, granted;
  Clock::time_point deadline{};
  size_t currentGeneration = 0, maximum = std::numeric_limits<size_t>::max();
  bool hasGeneration = false, pending = false;
};
} // namespace viewer3d::resources
