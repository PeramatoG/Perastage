#include "symbols/FixtureSymbolDiagnostics.h"

#include <locale>
#include <sstream>

namespace symbols {
namespace {

constexpr std::array<std::string_view,
                     static_cast<size_t>(FixtureSymbolPhase::Count)>
    kPhaseNames = {"resolve",     "fingerprint",     "inspect",
                   "bounds",      "capture",         "vectorization",
                   "calibration", "archive_rewrite", "validation",
                   "refresh"};

// Returns the stable diagnostic name for a final fixture-symbol outcome.
std::string_view OutcomeName(FixtureSymbolOutcome outcome) {
  switch (outcome) {
  case FixtureSymbolOutcome::Skipped:
    return "skipped";
  case FixtureSymbolOutcome::Generated:
    return "generated";
  case FixtureSymbolOutcome::Failed:
    return "failed";
  }
  return "failed";
}

} // namespace

// Starts an optionally enabled, independently owned fixture-symbol timing
// record.
FixtureSymbolTimings::FixtureSymbolTimings(bool enabled)
    : enabled_(enabled),
      started_(enabled ? Clock::now() : Clock::time_point{}) {}

// Creates an enabled timing record with deterministic time points for tests.
FixtureSymbolTimings::FixtureSymbolTimings(Clock::time_point started,
                                           Clock::time_point current)
    : enabled_(true), started_(started), controlledCurrent_(current) {}

// Accumulates one completed phase duration when diagnostics are enabled.
void FixtureSymbolTimings::Add(FixtureSymbolPhase phase, Duration elapsed) {
  if (!enabled_)
    return;
  auto &value = elapsed_[static_cast<size_t>(phase)];
  value = value.value_or(Duration::zero()) + elapsed;
}

// Reports whether a phase executed at least once.
bool FixtureSymbolTimings::Has(FixtureSymbolPhase phase) const {
  return elapsed_[static_cast<size_t>(phase)].has_value();
}

// Returns a phase duration, using zero for an absent phase.
FixtureSymbolTimings::Duration
FixtureSymbolTimings::Elapsed(FixtureSymbolPhase phase) const {
  return elapsed_[static_cast<size_t>(phase)].value_or(Duration::zero());
}

// Returns wall-clock time since this enabled timing record was created.
FixtureSymbolTimings::Duration FixtureSymbolTimings::Total() const {
  if (!enabled_)
    return Duration::zero();
  return std::chrono::duration_cast<Duration>(CurrentTime() - started_);
}

// Reports whether timing collection is enabled.
bool FixtureSymbolTimings::Enabled() const { return enabled_; }

// Returns the controlled test time or the production steady-clock time.
FixtureSymbolTimings::Clock::time_point
FixtureSymbolTimings::CurrentTime() const {
  if (controlledCurrent_)
    return *controlledCurrent_;
  return Clock::now();
}

// Formats one compact locale-independent diagnostic in stable phase order.
std::string FixtureSymbolTimings::Format(std::string_view fixtureLabel,
                                         std::string_view workKey,
                                         FixtureSymbolOutcome outcome) const {
  std::ostringstream output;
  output.imbue(std::locale::classic());
  output << "fixture_symbol label=\"" << fixtureLabel << "\" key=\"" << workKey
         << "\" outcome=" << OutcomeName(outcome)
         << " total_us=" << Total().count();
  for (size_t index = 0; index < kPhaseNames.size(); ++index) {
    output << ' ' << kPhaseNames[index] << "_us=";
    if (elapsed_[index])
      output << elapsed_[index]->count();
    else
      output << '-';
  }
  return output.str();
}

// Returns the exact stable name of a fixture-symbol phase.
std::string_view FixtureSymbolTimings::PhaseName(FixtureSymbolPhase phase) {
  return kPhaseNames[static_cast<size_t>(phase)];
}

// Begins timing one production phase only when a timing sink is enabled.
ScopedFixtureSymbolPhase::ScopedFixtureSymbolPhase(
    FixtureSymbolTimings *timings, FixtureSymbolPhase phase)
    : timings_(timings && timings->Enabled() ? timings : nullptr),
      phase_(phase) {
  if (timings_)
    started_ = timings_->CurrentTime();
}

// Accumulates the elapsed phase duration into its optional timing sink.
ScopedFixtureSymbolPhase::~ScopedFixtureSymbolPhase() { Finish(); }

// Finishes an active phase once and makes subsequent finishes no-ops.
void ScopedFixtureSymbolPhase::Finish() {
  if (!timings_)
    return;
  timings_->Add(phase_,
                std::chrono::duration_cast<FixtureSymbolTimings::Duration>(
                    timings_->CurrentTime() - started_));
  timings_ = nullptr;
}

// Returns the immutable capture mapping shared by runtime capture and tests.
const std::array<FixtureSymbolCaptureStep, 4> &FixtureSymbolCapturePlan() {
  static constexpr std::array<FixtureSymbolCaptureStep, 4> plan = {{
      {SymbolCaptureViewerView::Front, SymbolView::Front, false, false},
      {SymbolCaptureViewerView::Top, SymbolView::Top, false, false},
      {SymbolCaptureViewerView::Side, SymbolView::Left, false, true},
      {SymbolCaptureViewerView::Top, SymbolView::Bottom, true, false},
  }};
  return plan;
}

} // namespace symbols
