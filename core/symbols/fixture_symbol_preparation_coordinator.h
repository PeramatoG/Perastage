#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace symbols {

struct FixtureSymbolPreparationKey {
  std::string effectiveGdtfPath;
  std::string exactGdtfMode;

  bool operator==(const FixtureSymbolPreparationKey &) const = default;
};

struct FixtureSymbolPreparationKeyHash {
  std::size_t operator()(const FixtureSymbolPreparationKey &key) const;
};

enum class FixtureSymbolPreparationPriority { Automatic, Manual };
enum class FixtureSymbolPreparationState {
  Queued,
  Capturing,
  Processing,
  Publishing,
  Completed,
  Failed,
  Cancelled,
  Stale,
};

struct FixtureSymbolPreparationJob {
  FixtureSymbolPreparationKey key;
  std::string canonicalTarget;
  std::uint64_t epoch = 0;
  FixtureSymbolPreparationPriority priority =
      FixtureSymbolPreparationPriority::Automatic;
  FixtureSymbolPreparationState state = FixtureSymbolPreparationState::Queued;
  std::size_t captureStep = 0;
};

class FixtureSymbolPreparationCoordinator {
public:
  std::uint64_t BeginProjectEpoch();
  bool Request(const FixtureSymbolPreparationKey &key,
               std::string canonicalTarget,
               FixtureSymbolPreparationPriority priority);
  std::optional<FixtureSymbolPreparationJob> NextQueued();
  bool BeginCapture(const FixtureSymbolPreparationKey &key,
                    std::uint64_t epoch);
  bool CompleteCaptureStep(const FixtureSymbolPreparationKey &key,
                           std::uint64_t epoch);
  bool BeginPublishing(const FixtureSymbolPreparationKey &key,
                       std::uint64_t epoch);
  bool Complete(const FixtureSymbolPreparationKey &key, std::uint64_t epoch,
                bool success);
  bool Fail(const FixtureSymbolPreparationKey &key, std::uint64_t epoch);
  bool Skip(const FixtureSymbolPreparationKey &key, std::uint64_t epoch);
  bool IsCurrent(std::uint64_t epoch) const;
  std::size_t PendingCount() const;
  const FixtureSymbolPreparationJob *
  Find(const FixtureSymbolPreparationKey &key) const;

private:
  using Jobs = std::unordered_map<FixtureSymbolPreparationKey,
                                  FixtureSymbolPreparationJob,
                                  FixtureSymbolPreparationKeyHash>;
  Jobs jobs_;
  std::uint64_t epoch_ = 0;
  std::unordered_map<std::string, FixtureSymbolPreparationKey>
      publishingTargets_;
};

} // namespace symbols
