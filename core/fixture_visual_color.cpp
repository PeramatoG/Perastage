#include "fixture_visual_color.h"

#include <cctype>

// Builds the stable fixture-type key used when assigning shared automatic colors.
std::string BuildFixtureVisualColorGroupKey(const Fixture &fixture) {
  if (!fixture.gdtfSpec.empty())
    return "gdtf\n" + fixture.gdtfSpec + "\n" + fixture.gdtfMode;

  std::string normalizedType;
  normalizedType.reserve(fixture.typeName.size());
  for (const unsigned char value : fixture.typeName) {
    if (!std::isspace(value))
      normalizedType.push_back(static_cast<char>(std::tolower(value)));
  }
  return "type\n" + normalizedType + "\n" + fixture.gdtfMode;
}

// Normalizes a hexadecimal RGB color to canonical uppercase #RRGGBB form.
std::optional<std::string> NormalizeFixtureVisualColor(std::string_view raw) {
  size_t first = 0;
  size_t last = raw.size();
  while (first < last && std::isspace(static_cast<unsigned char>(raw[first])))
    ++first;
  while (last > first &&
         std::isspace(static_cast<unsigned char>(raw[last - 1])))
    --last;
  if (last > first && raw[first] == '#')
    ++first;
  if (last - first != 6)
    return std::nullopt;

  std::string normalized = "#";
  normalized.reserve(7);
  for (size_t index = first; index < last; ++index) {
    const unsigned char value = static_cast<unsigned char>(raw[index]);
    if (!std::isxdigit(value))
      return std::nullopt;
    normalized.push_back(static_cast<char>(std::toupper(value)));
  }
  return normalized;
}

// Resolves one fixture color according to project, recovery, and automatic precedence.
FixtureVisualColorResult
ResolveFixtureVisualColor(const FixtureVisualColorInput &input) {
  FixtureVisualColorResult result;
  if (const auto project = NormalizeFixtureVisualColor(input.projectColorHex)) {
    result.source = FixtureVisualColorSource::ProjectInstance;
    result.colorHex = project;
    return result;
  }
  result.hadInvalidInput = !input.projectColorHex.empty();

  if (input.projectState == FixtureProjectColorState::ExplicitEmpty) {
    result.source = FixtureVisualColorSource::ExplicitEmpty;
    return result;
  }
  if (input.allowLegacyMvrRecovery &&
      input.projectState == FixtureProjectColorState::Missing) {
    if (const auto recovered =
            NormalizeFixtureVisualColor(input.mvrFixtureColorHex)) {
      result.source = FixtureVisualColorSource::LegacyMvrRecovery;
      result.colorHex = recovered;
      return result;
    }
    result.hadInvalidInput =
        result.hadInvalidInput || !input.mvrFixtureColorHex.empty();
  }
  if (const auto automatic =
          NormalizeFixtureVisualColor(input.automaticColorHex)) {
    result.source = FixtureVisualColorSource::AutomaticFixtureType;
    result.colorHex = automatic;
    return result;
  }
  result.hadInvalidInput =
      result.hadInvalidInput || !input.automaticColorHex.empty();
  return result;
}

// Resolves presentation color from the durable state stored by one fixture.
FixtureVisualColorResult ResolveFixturePresentationColor(const Fixture &fixture) {
  FixtureProjectColorState state = fixture.visualColorState;
  if (!fixture.visualColorHex.empty())
    state = FixtureProjectColorState::Present;
  return ResolveFixtureVisualColor({fixture.visualColorHex, {},
                                    fixture.automaticVisualColorHex, state, false});
}

// Persists an automatic fallback only when project metadata is genuinely missing.
void PersistAutomaticFixtureVisualColor(Fixture &fixture, std::string_view colorHex) {
  if (fixture.visualColorState != FixtureProjectColorState::Missing ||
      !fixture.visualColorHex.empty())
    return;
  const auto normalized = NormalizeFixtureVisualColor(colorHex);
  if (!normalized)
    return;
  fixture.visualColorHex = *normalized;
  fixture.visualColorState = FixtureProjectColorState::Present;
}

// Aggregates resolved colors without depending on fixture iteration order.
FixtureVisualColorAggregate AggregateFixtureVisualColors(
    const std::vector<FixtureVisualColorResult> &colors) {
  FixtureVisualColorAggregate aggregate;
  if (colors.empty())
    return aggregate;

  const auto &first = colors.front();
  aggregate.colorHex = first.colorHex;
  aggregate.source = first.source;
  for (const auto &color : colors) {
    if (color.colorHex != aggregate.colorHex ||
        (!color.colorHex && color.source != aggregate.source)) {
      aggregate.colorHex.reset();
      aggregate.mixed = true;
      aggregate.source = FixtureVisualColorSource::Unresolved;
      return aggregate;
    }
  }
  return aggregate;
}
