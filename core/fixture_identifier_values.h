/* This file is part of Perastage. */
#pragma once
#include "fixture.h"
namespace fixture_identity {
struct Values { std::string text; int numeric = 0; };
inline Values Resolve(const Fixture &fixture) {
  if (fixture.identifierSource) {
    if (fixture.fixtureId != fixture.identifierSource->selectionIdAtImport)
      return {std::to_string(fixture.fixtureId), fixture.fixtureId};
    return {fixture.fixtureIdText, fixture.fixtureIdNumeric};
  }
  const int numeric = fixture.fixtureId != 0 ? fixture.fixtureId : fixture.fixtureIdNumeric;
  const bool aliasEdited = fixture.fixtureId != 0 && fixture.fixtureId != fixture.fixtureIdNumeric;
  const auto text = aliasEdited ? std::to_string(numeric) : fixture.fixtureIdText;
  return {text.empty() && numeric != 0 ? std::to_string(numeric) : text, numeric};
}
} // namespace fixture_identity
