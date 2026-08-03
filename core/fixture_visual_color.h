#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "../models/fixture.h"

enum class FixtureVisualColorSource {
  ProjectInstance,
  LegacyMvrRecovery,
  AutomaticFixtureType,
  ExplicitEmpty,
  Unresolved
};

struct FixtureVisualColorInput {
  std::string_view projectColorHex;
  std::string_view mvrFixtureColorHex;
  std::string_view automaticColorHex;
  FixtureProjectColorState projectState = FixtureProjectColorState::Missing;
  bool allowLegacyMvrRecovery = false;
};

struct FixtureVisualColorResult {
  FixtureVisualColorSource source = FixtureVisualColorSource::Unresolved;
  std::optional<std::string> colorHex;
  bool hadInvalidInput = false;
};

struct FixtureVisualColorAggregate {
  std::optional<std::string> colorHex;
  bool mixed = false;
  FixtureVisualColorSource source = FixtureVisualColorSource::Unresolved;
};

// Normalizes a hexadecimal RGB color to canonical uppercase #RRGGBB form.
std::optional<std::string> NormalizeFixtureVisualColor(std::string_view raw);

// Resolves one fixture color according to project, recovery, and automatic precedence.
FixtureVisualColorResult
ResolveFixtureVisualColor(const FixtureVisualColorInput &input);

// Resolves presentation color from the durable state stored by one fixture.
FixtureVisualColorResult ResolveFixturePresentationColor(const Fixture &fixture);

// Persists an automatic fallback only when project metadata is genuinely missing.
void PersistAutomaticFixtureVisualColor(Fixture &fixture, std::string_view colorHex);

// Aggregates resolved colors without depending on fixture iteration order.
FixtureVisualColorAggregate AggregateFixtureVisualColors(
    const std::vector<FixtureVisualColorResult> &colors);
