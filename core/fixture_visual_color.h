#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

enum class FixtureVisualColorSource {
  ProjectInstance,
  LegacyMvrRecovery,
  AutomaticFixtureType,
  ExplicitEmpty,
  Unresolved
};

enum class FixtureProjectColorState { Missing, Present, ExplicitEmpty };

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

// Aggregates resolved colors without depending on fixture iteration order.
FixtureVisualColorAggregate AggregateFixtureVisualColors(
    const std::vector<FixtureVisualColorResult> &colors);
