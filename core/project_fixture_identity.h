#pragma once

#include "json.hpp"
#include "mvrscene.h"
#include "uuidutils.h"

#include <algorithm>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace project_identity {

inline constexpr const char *kFixtureLabelOverridesConfigKey =
    "label_fixture_overrides";

struct FixtureMetadataNormalizationResult {
  std::optional<std::string> serializedOverrides;
  size_t migratedCount = 0;
  size_t collisionCount = 0;
  size_t staleCount = 0;
  bool changed = false;
};

namespace detail {

// Merges recoverable legacy values without replacing canonical values.
inline void MergeMissingJsonValues(nlohmann::json &canonical,
                                   const nlohmann::json &legacy) {
  if (canonical.is_object() && legacy.is_object()) {
    for (auto it = legacy.begin(); it != legacy.end(); ++it) {
      auto canonicalIt = canonical.find(it.key());
      if (canonicalIt == canonical.end())
        canonical[it.key()] = it.value();
      else
        MergeMissingJsonValues(*canonicalIt, it.value());
    }
    return;
  }
  if (canonical.is_array() && legacy.is_array()) {
    for (size_t index = 0; index < legacy.size(); ++index) {
      if (index >= canonical.size())
        canonical.push_back(legacy[index]);
      else if (canonical[index].is_null())
        canonical[index] = legacy[index];
      else
        MergeMissingJsonValues(canonical[index], legacy[index]);
    }
  }
}

} // namespace detail

// Collects unique fixture identities that remain provable after canonicalization.
inline std::unordered_set<std::string> CollectRecoverableFixtureUuids(
    const MvrScene &scene) {
  std::unordered_map<std::string, size_t> canonicalCounts;
  canonicalCounts.reserve(scene.fixtures.size());
  for (const auto &[key, fixture] : scene.fixtures) {
    (void)key;
    const std::string canonical = CanonicalizeUuid(fixture.uuid);
    if (!canonical.empty())
      ++canonicalCounts[canonical];
  }

  std::unordered_set<std::string> fixtureUuids;
  fixtureUuids.reserve(canonicalCounts.size());
  for (const auto &[canonical, count] : canonicalCounts) {
    if (count == 1)
      fixtureUuids.insert(canonical);
  }
  return fixtureUuids;
}

// Canonicalizes fixture-keyed project metadata against serialized scene identities.
inline FixtureMetadataNormalizationResult NormalizeFixtureLabelOverrides(
    const std::optional<std::string> &serializedOverrides,
    const std::unordered_set<std::string> &sceneFixtureUuids) {
  FixtureMetadataNormalizationResult result;
  if (!serializedOverrides || serializedOverrides->empty())
    return result;

  const nlohmann::json source =
      nlohmann::json::parse(*serializedOverrides, nullptr, false);
  if (!source.is_object()) {
    result.serializedOverrides = serializedOverrides;
    return result;
  }

  std::vector<std::string> keys;
  keys.reserve(source.size());
  for (auto it = source.begin(); it != source.end(); ++it)
    keys.push_back(it.key());
  std::sort(keys.begin(), keys.end());

  nlohmann::json normalized = nlohmann::json::object();
  std::stable_sort(keys.begin(), keys.end(), [&](const std::string &lhs,
                                                  const std::string &rhs) {
    const bool lhsCanonical = sceneFixtureUuids.contains(lhs);
    const bool rhsCanonical = sceneFixtureUuids.contains(rhs);
    return lhsCanonical != rhsCanonical ? lhsCanonical : lhs < rhs;
  });

  for (const std::string &key : keys) {
    if (!source.at(key).is_object())
      continue;
    const std::string canonical = CanonicalizeUuid(key);
    const std::string target = sceneFixtureUuids.contains(key)
                                   ? key
                                   : canonical;
    if (target.empty() || !sceneFixtureUuids.contains(target)) {
      ++result.staleCount;
      result.changed = true;
      continue;
    }

    auto existing = normalized.find(target);
    if (existing == normalized.end()) {
      normalized[target] = source.at(key);
    } else {
      ++result.collisionCount;
      detail::MergeMissingJsonValues(*existing, source.at(key));
    }
    if (key != target) {
      ++result.migratedCount;
      result.changed = true;
    }
  }

  if (normalized.empty()) {
    result.changed = result.changed || !source.empty();
    return result;
  }
  result.serializedOverrides = normalized.dump();
  result.changed = result.changed || result.collisionCount > 0 ||
                   *result.serializedOverrides != *serializedOverrides;
  return result;
}

} // namespace project_identity
