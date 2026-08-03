#include "fixture_visual_color.h"

#include <cassert>

// Verifies resolver precedence, normalization, recovery, and empty semantics.
static void TestResolution() {
  auto result = ResolveFixtureVisualColor(
      {" #a1b2c3 ", "#445566", "#778899",
       FixtureProjectColorState::Present, true});
  assert(result.source == FixtureVisualColorSource::ProjectInstance);
  assert(result.colorHex == "#A1B2C3");

  result = ResolveFixtureVisualColor(
      {{}, "#12ab34", "#778899", FixtureProjectColorState::Missing, true});
  assert(result.source == FixtureVisualColorSource::LegacyMvrRecovery);
  assert(result.colorHex == "#12AB34");

  result = ResolveFixtureVisualColor(
      {{}, "#12AB34", "#778899", FixtureProjectColorState::ExplicitEmpty,
       true});
  assert(result.source == FixtureVisualColorSource::ExplicitEmpty);
  assert(!result.colorHex);

  result = ResolveFixtureVisualColor(
      {{}, {}, "778899", FixtureProjectColorState::Missing, false});
  assert(result.source == FixtureVisualColorSource::AutomaticFixtureType);
  assert(result.colorHex == "#778899");

  result = ResolveFixtureVisualColor(
      {{}, {}, "#778899", FixtureProjectColorState::ExplicitEmpty, false});
  assert(result.source == FixtureVisualColorSource::ExplicitEmpty);

  result = ResolveFixtureVisualColor(
      {"broken", "also-broken", "#xyzxyz", FixtureProjectColorState::Present,
       false});
  assert(result.source == FixtureVisualColorSource::Unresolved);
  assert(!result.colorHex);
  assert(result.hadInvalidInput);

  result = ResolveFixtureVisualColor(
      {"broken", "#123456", "#ABCDEF", FixtureProjectColorState::Present,
       true});
  assert(result.source == FixtureVisualColorSource::AutomaticFixtureType);

  Fixture explicitEmpty;
  explicitEmpty.visualColorState = FixtureProjectColorState::ExplicitEmpty;
  explicitEmpty.mvrFixtureColorHex = "#123456";
  explicitEmpty.automaticVisualColorHex = "#ABCDEF";
  assert(ResolveFixturePresentationColor(explicitEmpty).source ==
         FixtureVisualColorSource::ExplicitEmpty);
  PersistAutomaticFixtureVisualColor(explicitEmpty, "#654321");
  assert(explicitEmpty.visualColorHex.empty());

  Fixture missing;
  PersistAutomaticFixtureVisualColor(missing, "#abcdef");
  assert(missing.visualColorHex == "#ABCDEF");
}

// Verifies deterministic uniform, mixed, empty, and unresolved aggregation.
static void TestAggregation() {
  const auto red = ResolveFixtureVisualColor(
      {"#ff0000", {}, {}, FixtureProjectColorState::Present, false});
  const auto redAgain = ResolveFixtureVisualColor(
      {"FF0000", {}, {}, FixtureProjectColorState::Present, false});
  const auto blue = ResolveFixtureVisualColor(
      {"#0000ff", {}, {}, FixtureProjectColorState::Present, false});
  const auto empty = ResolveFixtureVisualColor(
      {{}, {}, {}, FixtureProjectColorState::ExplicitEmpty, false});
  const auto unresolved = ResolveFixtureVisualColor({});

  auto aggregate = AggregateFixtureVisualColors({red, redAgain});
  assert(!aggregate.mixed && aggregate.colorHex == "#FF0000");
  aggregate = AggregateFixtureVisualColors({red, blue});
  assert(aggregate.mixed && !aggregate.colorHex);
  aggregate = AggregateFixtureVisualColors({red, empty});
  assert(aggregate.mixed && !aggregate.colorHex);
  aggregate = AggregateFixtureVisualColors({unresolved, unresolved});
  assert(!aggregate.mixed && !aggregate.colorHex);
}

// Runs the fixture visual-color domain regression checks.
int main() {
  TestResolution();
  TestAggregation();
  return 0;
}
