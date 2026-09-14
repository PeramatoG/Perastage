/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
#include "mvr_export_preparation.h"

#include "mvr_identity_recovery.h"
#include "scene_transform_integrity.h"
#include "uuidutils.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <set>
#include <unordered_set>

namespace mvr_export_preparation {
namespace {

struct FixtureExportId {
  std::string text;
  int numeric = 0;
  bool preserveTextOnNumericRepair = false;
};

// Trims ASCII whitespace without applying locale-dependent transformations.
std::string TrimAscii(std::string value) {
  const auto first =
      std::find_if_not(value.begin(), value.end(),
                       [](unsigned char ch) { return std::isspace(ch); });
  const auto last =
      std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) {
        return std::isspace(ch);
      }).base();
  return first < last ? std::string(first, last) : std::string{};
}

// Resolves the established MVR FixtureID compatibility representation.
FixtureExportId ResolveFixtureExportId(const Fixture &fixture) {
  FixtureExportId id;
  id.numeric =
      fixture.fixtureId > 0 ? fixture.fixtureId : fixture.fixtureIdNumeric;
  if (id.numeric <= 0)
    id.numeric = 0;
  if (fixture.fixtureIdNumeric > 0 &&
      fixture.fixtureId == fixture.fixtureIdNumeric)
    id.text = TrimAscii(fixture.fixtureIdText);
  id.preserveTextOnNumericRepair =
      !id.text.empty() && id.text != std::to_string(id.numeric);
  if (id.text.empty() && id.numeric > 0)
    id.text = std::to_string(id.numeric);
  return id;
}

// Builds the normalized fixture type key used for UnitNumber grouping.
std::string BuildFixtureUnitNumberTypeKey(const Fixture &fixture) {
  auto normalize = [](std::string value) {
    value = TrimAscii(std::move(value));
    std::string normalized;
    bool pendingSpace = false;
    for (unsigned char ch : value) {
      if (std::isspace(ch)) {
        pendingSpace = !normalized.empty();
        continue;
      }
      if (pendingSpace)
        normalized.push_back(' ');
      pendingSpace = false;
      normalized.push_back(static_cast<char>(ch));
    }
    return normalized;
  };
  for (const std::string *candidate :
       {&fixture.typeName, &fixture.gdtfSpec, &fixture.requestedFixtureName,
        &fixture.instanceName}) {
    std::string key = normalize(*candidate);
    if (!key.empty())
      return key;
  }
  return "Unknown";
}

// Assigns stable positive UnitNumber values while preserving explicit values.
std::unordered_map<std::string, int> BuildFixtureUnitNumbers(
    const std::unordered_map<std::string, Fixture> &fixtures) {
  struct FixtureRef {
    std::string uuid;
    const Fixture *fixture;
  };
  struct UnitGroup {
    std::set<int> used;
    std::vector<FixtureRef> missing;
  };
  std::unordered_map<std::string, int> result;
  std::unordered_map<std::string, UnitGroup> groups;
  for (const auto &[uuid, fixture] : fixtures) {
    UnitGroup &group = groups[BuildFixtureUnitNumberTypeKey(fixture)];
    if (fixture.unitNumber > 0) {
      group.used.insert(fixture.unitNumber);
      result[uuid] = fixture.unitNumber;
    } else {
      group.missing.push_back({uuid, &fixture});
    }
  }
  constexpr float kPositionTieTolerance = 0.0001f;
  for (auto &[key, group] : groups) {
    (void)key;
    std::sort(group.missing.begin(), group.missing.end(),
              [](const FixtureRef &lhs, const FixtureRef &rhs) {
                const auto lhsPos = lhs.fixture->GetPosition();
                const auto rhsPos = rhs.fixture->GetPosition();
                if (std::fabs(lhsPos[1] - rhsPos[1]) > kPositionTieTolerance)
                  return lhsPos[1] < rhsPos[1];
                if (std::fabs(lhsPos[0] - rhsPos[0]) > kPositionTieTolerance)
                  return lhsPos[0] < rhsPos[0];
                const int lhsId = ResolveFixtureExportId(*lhs.fixture).numeric;
                const int rhsId = ResolveFixtureExportId(*rhs.fixture).numeric;
                if (lhsId != rhsId) {
                  if (lhsId <= 0)
                    return false;
                  if (rhsId <= 0)
                    return true;
                  return lhsId < rhsId;
                }
                if (lhs.fixture->instanceName != rhs.fixture->instanceName)
                  return lhs.fixture->instanceName < rhs.fixture->instanceName;
                return lhs.uuid < rhs.uuid;
              });
    int next = 1;
    for (const FixtureRef &fixture : group.missing) {
      while (group.used.contains(next))
        ++next;
      result[fixture.uuid] = next;
      group.used.insert(next++);
    }
  }
  return result;
}

// Returns map keys in deterministic lexical order.
template <typename Map> std::vector<std::string> SortedKeys(const Map &map) {
  std::vector<std::string> keys;
  keys.reserve(map.size());
  for (const auto &[key, value] : map) {
    (void)value;
    keys.push_back(key);
  }
  std::sort(keys.begin(), keys.end());
  return keys;
}

// Converts identity-recovery details into the export diagnostic contract.
void AppendIdentityDiagnostics(Result &result,
                               const mvridentity::RecoveryResult &recovery) {
  std::unordered_set<std::string> emittedWarnings;
  for (const auto &diagnostic : recovery.diagnostics) {
    const bool conflict = diagnostic.conflictingValidIdentities;
    const bool visibleCandidate =
        diagnostic.reason == mvridentity::RecoveryReason::Duplicate ||
        conflict ||
        diagnostic.reason == mvridentity::RecoveryReason::AmbiguousReference ||
        diagnostic.reason == mvridentity::RecoveryReason::UnresolvedReference ||
        diagnostic.generatedNewIdentity;
    const std::string eventKey = diagnostic.objectKind + "|" +
                                 diagnostic.objectName + "|" +
                                 diagnostic.replacementIdentity;
    const bool visible =
        visibleCandidate && emittedWarnings.insert(eventKey).second;
    const auto code =
        visible
            ? (diagnostic.reason == mvridentity::RecoveryReason::Duplicate
                   ? MvrExportDiagnosticCode::IdentityReassigned
               : conflict ? MvrExportDiagnosticCode::IdentityConflict
               : diagnostic.reason ==
                           mvridentity::RecoveryReason::AmbiguousReference ||
                       diagnostic.reason ==
                           mvridentity::RecoveryReason::UnresolvedReference
                   ? MvrExportDiagnosticCode::ReferenceCleared
                   : MvrExportDiagnosticCode::IdentityGenerated)
        : diagnostic.reason == mvridentity::RecoveryReason::InferredLayer
            ? MvrExportDiagnosticCode::LayerInferred
        : diagnostic.reason == mvridentity::RecoveryReason::Canonicalized
            ? MvrExportDiagnosticCode::IdentityCanonicalized
            : MvrExportDiagnosticCode::InternalRecovery;
    result.diagnostics.push_back(
        {code,
         visible ? MvrExportDiagnosticSeverity::Warning
                 : MvrExportDiagnosticSeverity::Info,
         visible ? MvrExportDiagnosticImpact::IdentityChanged
                 : MvrExportDiagnosticImpact::None,
         visible,
         diagnostic.objectKind,
         diagnostic.objectName,
         diagnostic.replacementIdentity,
         {},
         mvridentity::FormatRecoveryDiagnostic(diagnostic)});
  }
}

// Prepares globally unique object fixture identifiers in stable object order.
void PrepareObjectIds(Result &result) {
  int nextId = 1;
  std::unordered_set<int> reserved;
  std::unordered_set<int> assignedFixtures;
  std::unordered_set<int> used;
  for (const auto &uuid : SortedKeys(result.scene.fixtures)) {
    const int candidate =
        ResolveFixtureExportId(result.scene.fixtures.at(uuid)).numeric;
    if (candidate > 0)
      reserved.insert(candidate);
  }
  used = reserved;
  auto allocate = [&]() {
    while (used.contains(nextId))
      ++nextId;
    used.insert(nextId);
    return nextId++;
  };
  for (const auto &uuid : SortedKeys(result.scene.fixtures)) {
    const Fixture &fixture = result.scene.fixtures.at(uuid);
    FixtureExportId id = ResolveFixtureExportId(fixture);
    if (id.numeric > 0 && !assignedFixtures.insert(id.numeric).second) {
      const int original = id.numeric;
      id.numeric = allocate();
      if (!id.preserveTextOnNumericRepair)
        id.text = std::to_string(id.numeric);
      std::string name = TrimAscii(fixture.instanceName);
      if (name.empty())
        name = fixture.uuid.empty() ? "unnamed fixture" : fixture.uuid;
      result.objectIdDiagnostics.push_back(
          {MvrExportDiagnosticCode::FixtureIdReassigned,
           MvrExportDiagnosticSeverity::Warning,
           MvrExportDiagnosticImpact::IdentityChanged,
           true,
           "Fixture",
           name,
           fixture.uuid,
           {},
           "MVR export reassigned duplicate FixtureIDNumeric " +
               std::to_string(original) + " for fixture '" + name +
               "' to the next available value " + std::to_string(id.numeric) +
               "."});
    } else if (id.numeric <= 0) {
      id.numeric = allocate();
      id.text = std::to_string(id.numeric);
    }
    if (id.text.empty())
      id.text = std::to_string(id.numeric);
    result.objectIds[uuid] = {id.text, id.numeric};
  }
  for (const auto &uuid : SortedKeys(result.scene.trusses)) {
    const Truss &object = result.scene.trusses.at(uuid);
    const int numeric = allocate();
    const std::string text = TrimAscii(object.name);
    result.objectIds[uuid] = {text.empty() ? std::to_string(numeric) : text,
                              numeric};
  }
  for (const auto &uuid : SortedKeys(result.scene.supports)) {
    const Support &object = result.scene.supports.at(uuid);
    const int numeric = allocate();
    const std::string text = TrimAscii(object.name);
    result.objectIds[uuid] = {text.empty() ? std::to_string(numeric) : text,
                              numeric};
  }
  for (const auto &uuid : SortedKeys(result.scene.sceneObjects)) {
    const SceneObject &object = result.scene.sceneObjects.at(uuid);
    const int numeric = object.fixtureIdNumeric > 0 &&
                                used.insert(object.fixtureIdNumeric).second
                            ? object.fixtureIdNumeric
                            : allocate();
    const std::string text = TrimAscii(object.fixtureIdText);
    result.objectIds[uuid] = {text.empty() ? std::to_string(numeric) : text,
                              numeric};
  }
}

// Prepares canonical position definitions and resolved object references.
void PreparePositions(Result &result) {
  std::unordered_map<std::string, std::string> legacyToCanonical;
  std::unordered_set<std::string> used;
  auto reserve = [&](const std::string &candidate, const std::string &seed) {
    std::string value = CanonicalizeUuid(candidate);
    if (value.empty() || used.contains(value)) {
      int suffix = 0;
      do {
        value = DeriveDeterministicUuid(seed + "#" + std::to_string(suffix++));
      } while (used.contains(value));
    }
    used.insert(value);
    return value;
  };
  for (const auto &[rawUuid, rawName] : result.scene.positions) {
    const std::string name = TrimAscii(rawName);
    const std::string canonical = CanonicalizeUuid(rawUuid);
    if (!canonical.empty()) {
      const std::string stable = reserve(
          canonical, "mvr:position:canonical:" + canonical + ":" + name);
      result.positions[stable] = name;
      if (stable != rawUuid)
        legacyToCanonical[rawUuid] = stable;
    } else {
      const std::string generated =
          reserve({}, "mvr:position:legacy:" + rawUuid + ":" + name);
      result.positions[generated] = name.empty() ? rawUuid : name;
      legacyToCanonical[rawUuid] = generated;
      result.informationalLogs.push_back(
          "MVR export converted legacy Position uuid '" + rawUuid +
          "' to canonical '" + generated + "' (name='" +
          result.positions[generated] + "')");
    }
  }
  std::unordered_map<std::string, std::string> byName;
  for (const auto &[uuid, name] : result.positions)
    if (!name.empty())
      byName[name] = uuid;
  auto ensure = [&](const std::string &id, const std::string &name) {
    if (auto it = legacyToCanonical.find(id); it != legacyToCanonical.end()) {
      if (!name.empty()) {
        result.positions[it->second] = name;
        byName[name] = it->second;
      }
      return;
    }
    const std::string canonical = CanonicalizeUuid(id);
    if (!canonical.empty()) {
      auto [it, inserted] = result.positions.try_emplace(canonical, name);
      if (!inserted && !name.empty())
        it->second = name;
      if (!name.empty())
        byName.try_emplace(name, canonical);
      return;
    }
    if (name.empty() || byName.contains(name))
      return;
    const std::string generated = reserve({}, "mvr:position:name:" + name);
    result.positions[generated] = name;
    byName[name] = generated;
    if (!id.empty())
      result.informationalLogs.push_back(
          "MVR export normalized legacy Position uuid '" + id + "' -> '" +
          generated + "' (name='" + name + "')");
  };
  for (const auto &[uuid, object] : result.scene.fixtures)
    ensure(object.position, object.positionName);
  for (const auto &[uuid, object] : result.scene.trusses)
    ensure(object.position, object.positionName);
  for (const auto &[uuid, object] : result.scene.supports)
    ensure(object.position, object.positionName);
  auto resolve = [&](const std::string &id, const std::string &name,
                     bool &resolvedByName) {
    resolvedByName = false;
    if (auto it = legacyToCanonical.find(id); it != legacyToCanonical.end())
      return it->second;
    const std::string canonical = CanonicalizeUuid(id);
    if (!canonical.empty() && result.positions.contains(canonical))
      return canonical;
    if (auto it = byName.find(name); !name.empty() && it != byName.end()) {
      resolvedByName = true;
      return it->second;
    }
    return std::string{};
  };
  auto prepareReference = [&](const std::string &position,
                              const std::string &positionName,
                              const std::string &uuid, const std::string &name,
                              const char *type) {
    bool resolvedByName = false;
    const std::string resolved =
        resolve(position, positionName, resolvedByName);
    result.positionReferences[uuid] = resolved;
    if (resolvedByName && !position.empty() && resolved != position)
      result.positionReferenceInformationalLogs.emplace(
          uuid, "MVR export remapped non-canonical Position '" + position +
                    "' to '" + resolved + "' by name '" + positionName + "'");
    if (resolved.empty() && !TrimAscii(position).empty())
      result.positionReferenceDiagnostics.emplace(
          uuid,
          MvrExportDiagnostic{
              MvrExportDiagnosticCode::ReferenceCleared,
              MvrExportDiagnosticSeverity::Warning,
              MvrExportDiagnosticImpact::DataOmitted,
              true,
              type,
              name,
              uuid,
              {},
              "MVR export omitted an unresolved Position reference for " +
                  std::string(type) + " '" + name + "' (uuid=" + uuid + ")."});
  };
  for (const auto &[uuid, object] : result.scene.fixtures)
    prepareReference(object.position, object.positionName, object.uuid,
                     object.instanceName, "Fixture");
  for (const auto &[uuid, object] : result.scene.trusses)
    prepareReference(object.position, object.positionName, object.uuid,
                     object.name, "Truss");
  for (const auto &[uuid, object] : result.scene.supports)
    prepareReference(object.position, object.positionName, object.uuid,
                     object.name, "Support");
}

} // namespace

// Prepares an immutable source scene for export without package or XML I/O.
Result Prepare(const MvrScene &sourceScene, const MvrExportOptions &options) {
  (void)options;
  Result result;
  result.scene = sourceScene;
  const auto recovery =
      mvridentity::RecoverSceneIdentities(result.scene, "editable-scene");
  AppendIdentityDiagnostics(result, recovery);
  const auto integrity =
      scene_transform_integrity::ValidateAndRepair(result.scene);
  for (const auto &diagnostic : integrity.diagnostics) {
    const bool fatal =
        diagnostic.severity == scene_transform_integrity::Severity::Fatal;
    result.diagnostics.push_back(
        {fatal ? MvrExportDiagnosticCode::TransformInvalid
               : MvrExportDiagnosticCode::TransformRepaired,
         fatal ? MvrExportDiagnosticSeverity::Error
               : MvrExportDiagnosticSeverity::Info,
         fatal ? MvrExportDiagnosticImpact::ExportFailed
               : MvrExportDiagnosticImpact::None,
         fatal,
         {},
         {},
         diagnostic.uuid,
         {},
         scene_transform_integrity::FormatDiagnostic(diagnostic)});
  }
  if (!integrity.success)
    return result;
  PreparePositions(result);
  PrepareObjectIds(result);
  result.fixtureUnitNumbers = BuildFixtureUnitNumbers(result.scene.fixtures);
  for (const auto &[uuid, layer] : result.scene.layers)
    result.layerUuids[uuid] =
        uuid.empty() ? std::string{}
                     : (CanonicalizeUuid(uuid) == uuid
                            ? uuid
                            : DeriveDeterministicUuid(
                                  "mvr:layer:" + layer.name + ":" + uuid));
  result.success = true;
  return result;
}

} // namespace mvr_export_preparation
