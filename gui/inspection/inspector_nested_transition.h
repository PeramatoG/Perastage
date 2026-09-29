#pragma once

#include "inspection/inspector_source_context.h"

#include <cstdint>
#include <memory>

namespace gui::inspection {

// Retains the committed parent while one latest-wins nested load is pending.
class InspectorNestedTransition final {
public:
  void Begin(std::shared_ptr<const DisplayedPackageContext> parent,
             std::uint64_t sourceGeneration);
  void SetWorkerGeneration(std::uint64_t workerGeneration);
  bool Matches(std::uint64_t sourceGeneration) const;
  bool MatchesWorker(std::uint64_t workerGeneration) const;
  bool IsPending() const;
  std::shared_ptr<const DisplayedPackageContext> Commit();
  void Cancel();

private:
  std::shared_ptr<const DisplayedPackageContext> parent_;
  std::uint64_t sourceGeneration_ = 0;
  std::uint64_t workerGeneration_ = 0;
};

} // namespace gui::inspection
