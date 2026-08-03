#pragma once

#include <array>
#include <chrono>
#include <optional>
#include <string>
#include <string_view>

#include "Symbol2DTypes.h"

namespace symbols {

enum class FixtureSymbolPhase {
  Resolve,
  Fingerprint,
  Inspect,
  Bounds,
  Capture,
  Vectorization,
  Calibration,
  ArchiveRewrite,
  Validation,
  Refresh,
  Count,
};

enum class FixtureSymbolOutcome { Skipped, Generated, Failed };

class FixtureSymbolTimings {
public:
  using Clock = std::chrono::steady_clock;
  using Duration = std::chrono::microseconds;

  explicit FixtureSymbolTimings(bool enabled = false);
  FixtureSymbolTimings(Clock::time_point started, Clock::time_point current);
  void Add(FixtureSymbolPhase phase, Duration elapsed);
  bool Has(FixtureSymbolPhase phase) const;
  Duration Elapsed(FixtureSymbolPhase phase) const;
  Duration Total() const;
  bool Enabled() const;
  std::string Format(std::string_view fixtureLabel, std::string_view workKey,
                     FixtureSymbolOutcome outcome) const;
  static std::string_view PhaseName(FixtureSymbolPhase phase);

private:
  friend class ScopedFixtureSymbolPhase;
  Clock::time_point CurrentTime() const;

  bool enabled_ = false;
  Clock::time_point started_;
  std::optional<Clock::time_point> controlledCurrent_;
  std::array<std::optional<Duration>,
             static_cast<size_t>(FixtureSymbolPhase::Count)>
      elapsed_{};
};

class ScopedFixtureSymbolPhase {
public:
  ScopedFixtureSymbolPhase(FixtureSymbolTimings *timings,
                           FixtureSymbolPhase phase);
  ~ScopedFixtureSymbolPhase();
  void Finish();

private:
  FixtureSymbolTimings *timings_ = nullptr;
  FixtureSymbolPhase phase_ = FixtureSymbolPhase::Resolve;
  FixtureSymbolTimings::Clock::time_point started_;
};

enum class SymbolCaptureViewerView { Front, Top, Side };

struct FixtureSymbolCaptureStep {
  SymbolCaptureViewerView viewerView;
  SymbolView symbolView;
  bool forceBottomViewForTopFixtures;
  bool mirrorHorizontally;
};

const std::array<FixtureSymbolCaptureStep, 4> &FixtureSymbolCapturePlan();

} // namespace symbols
