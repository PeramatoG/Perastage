#pragma once

#include "mvrscene.h"
#include "project_fixture_symbol_runtime.h"

#include <cctype>
#include <functional>
#include <string_view>

namespace symbols {

inline bool IsFixtureSymbolPathKey(std::string_view key) {
  if (key.find_first_of("/\\") != std::string_view::npos)
    return true;
  constexpr std::string_view suffix = ".gdtf";
  if (key.size() < suffix.size())
    return false;
  key.remove_prefix(key.size() - suffix.size());
  for (size_t index = 0; index < suffix.size(); ++index)
    if (std::tolower(static_cast<unsigned char>(key[index])) != suffix[index])
      return false;
  return true;
}

// Captured paths and explicit project bindings are authoritative. Only old
// name-only captures may consult scene names, and names never select overrides.
inline std::string ResolveFixtureSymbolLookupSource(
    const std::string &modelKey, const std::string &sourceKey,
    const MvrScene &scene,
    const std::function<std::string(const Fixture &)> &legacyGdtfSource) {
  if (IsProjectFixtureSymbolSource(modelKey) || IsFixtureSymbolPathKey(modelKey))
    return modelKey;
  const auto resolve = [&](const Fixture &fixture) {
    const auto candidate = legacyGdtfSource(fixture);
    return IsProjectFixtureSymbolSource(candidate) ? std::string{} : candidate;
  };
  for (const auto &[uuid, fixture] : scene.fixtures) {
    (void)uuid;
    if (!sourceKey.empty() && fixture.typeName == sourceKey) {
      if (const auto candidate = resolve(fixture); !candidate.empty())
        return candidate;
    }
  }
  for (const auto &[uuid, fixture] : scene.fixtures) {
    (void)uuid;
    if (fixture.gdtfSpec == modelKey || fixture.typeName == modelKey) {
      if (const auto candidate = resolve(fixture); !candidate.empty())
        return candidate;
    }
  }
  return modelKey;
}

} // namespace symbols
