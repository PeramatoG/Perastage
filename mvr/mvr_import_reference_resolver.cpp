#include "mvr_import_reference_resolver.h"

#include "uuidutils.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace mvr {
namespace {

// Appends unknown metadata diagnostics in stable UUID order.
void DiagnoseUnknownMetadata(const std::unordered_set<std::string> &entries,
                             const std::unordered_set<std::string> &consumed,
                             const char *code, const char *kind,
                             MvrImportResult &result) {
  std::vector<std::string> unknown;
  for (const std::string &uuid : entries) {
    if (!consumed.contains(uuid))
      unknown.push_back(uuid);
  }
  std::sort(unknown.begin(), unknown.end());
  for (const std::string &uuid : unknown) {
    result.diagnostics.push_back(
        {code,
         std::string("Ignored ") + kind + " for unknown UUID '" + uuid + "'."});
  }
}

} // namespace

// Creates an import reference resolver with an optional compatibility warning
// sink.
MvrImportReferenceResolver::MvrImportReferenceResolver(WarningSink warningSink)
    : warningSink_(std::move(warningSink)) {}

// Builds the legacy-compatible deterministic seed for one imported identity.
std::string MvrImportReferenceResolver::StableSeed(
    const MvrImportedIdentity &identity) const {
  return "mvr:" + identity.kind + ':' + identity.layerName + ':' +
         identity.objectName + ':' + identity.transform + ':' +
         identity.rawUuid;
}

// Reports a non-fatal compatibility recovery warning when a sink is available.
void MvrImportReferenceResolver::Warn(const std::string &message) const {
  if (warningSink_)
    warningSink_(message);
}

// Canonicalizes or deterministically repairs one stable imported identity.
std::string MvrImportReferenceResolver::ResolveStableUuid(
    const MvrImportedIdentity &identity) {
  std::string stableUuid = CanonicalizeUuid(identity.rawUuid);
  const std::string seed = StableSeed(identity);
  if (stableUuid.empty()) {
    stableUuid = CanonicalizeUuid(identity.legacyStableId);
    if (stableUuid.empty() && !identity.rawUuid.empty()) {
      Warn("MVR import: " + identity.kind + " UUID '" + identity.rawUuid +
           "' is invalid. Applying deterministic fallback.");
    }
    if (stableUuid.empty())
      stableUuid = DeriveDeterministicUuid(seed);
  }

  if (usedStableUuids_.contains(stableUuid)) {
    Warn("MVR import: UUID collision for " + identity.kind + " '" + stableUuid +
         "'. Applying controlled fallback UUID.");
    int suffix = 1;
    std::string candidate;
    do {
      candidate =
          DeriveDeterministicUuid(seed + "#" + std::to_string(suffix++));
    } while (usedStableUuids_.contains(candidate));
    stableUuid = std::move(candidate);
  }
  usedStableUuids_.insert(stableUuid);
  return stableUuid;
}

// Returns the canonical or deterministic reference identity without reserving
// it.
std::string MvrImportReferenceResolver::ReferenceUuid(
    const MvrImportedIdentity &identity) const {
  const std::string canonical = CanonicalizeUuid(identity.rawUuid);
  return canonical.empty() ? DeriveDeterministicUuid(StableSeed(identity))
                           : canonical;
}

// Imports a standard Position UUID or isolates deterministic legacy recovery.
void MvrImportReferenceResolver::ImportPosition(const std::string &rawUuid,
                                                const std::string &name,
                                                MvrScene &scene) {
  const std::string canonical = CanonicalizeUuid(rawUuid);
  if (canonical.empty()) {
    if (rawUuid.empty())
      return;
    const std::string generated =
        DeriveDeterministicUuid("mvr:legacy-position:" + rawUuid + ":" + name);
    legacyPositionRemap_[rawUuid] = generated;
    scene.positions[generated] = name.empty() ? rawUuid : name;
    Warn("MVR import migrated non-canonical Position uuid '" + rawUuid +
         "' -> '" + generated + "'");
    return;
  }
  if (canonical != rawUuid)
    legacyPositionRemap_[rawUuid] = canonical;
  scene.positions[canonical] = name;
}

// Preserves an unresolved Position reference for a lossless subsequent export.
std::string
MvrImportReferenceResolver::EnsurePosition(const std::string &positionId,
                                           MvrScene &scene) {
  if (positionId.empty())
    return {};
  const auto legacy = legacyPositionRemap_.find(positionId);
  const std::string remapped =
      legacy == legacyPositionRemap_.end() ? positionId : legacy->second;
  const std::string canonical = CanonicalizeUuid(remapped);
  const std::string normalized = canonical.empty() ? remapped : canonical;
  const auto existing = scene.positions.find(normalized);
  if (existing != scene.positions.end())
    return existing->second;
  scene.positions[normalized] = positionId;
  return positionId;
}

// Records an imported fixture spelling that resolves to a final stable UUID.
void MvrImportReferenceResolver::RecordFixtureUuid(
    const std::string &rawUuid, const std::string &resolvedUuid) {
  if (!rawUuid.empty() && rawUuid != resolvedUuid)
    fixtureUuidRemap_[rawUuid] = resolvedUuid;
}

// Exposes fixture aliases through the narrow scene-reader state contract.
std::unordered_map<std::string, std::string> &
MvrImportReferenceResolver::FixtureUuidRemap() {
  return fixtureUuidRemap_;
}

// Exposes fixture aliases through the read-only import result contract.
const std::unordered_map<std::string, std::string> &
MvrImportReferenceResolver::FixtureUuidRemap() const {
  return fixtureUuidRemap_;
}

// Exposes recovered Position aliases as read-only scene metadata.
const std::unordered_map<std::string, std::string> &
MvrImportReferenceResolver::LegacyPositionRemap() const {
  return legacyPositionRemap_;
}

// Reconciles references that require a complete scene and diagnoses leftovers.
void MvrImportReferenceResolver::Reconcile(
    MvrImportResult &result,
    const MvrImportPostParseReferences &references) const {
  std::vector<std::string> supportUuids;
  for (const auto &[uuid, support] : result.scene.supports) {
    (void)support;
    supportUuids.push_back(uuid);
  }
  std::sort(supportUuids.begin(), supportUuids.end());
  for (const std::string &uuid : supportUuids) {
    Support &support = result.scene.supports.at(uuid);
    if (support.motorFixtureUuid.empty())
      continue;
    const std::string canonical = CanonicalizeUuid(support.motorFixtureUuid);
    if (canonical.empty())
      continue;
    const auto alias = fixtureUuidRemap_.find(support.motorFixtureUuid);
    const std::string resolved =
        alias == fixtureUuidRemap_.end() ? canonical : alias->second;
    if (result.scene.fixtures.contains(resolved)) {
      support.motorFixtureUuid = resolved;
    } else {
      result.diagnostics.push_back(
          {"unknown_motor_fixture_uuid",
           "Support '" + uuid + "' references an unknown MotorFixtureUuid."});
      support.motorFixtureUuid.clear();
    }
  }

  DiagnoseUnknownMetadata(references.hoistInfoUuids,
                          references.consumedHoistInfoUuids,
                          "unknown_hoist_info_uuid", "HoistInfo", result);
  DiagnoseUnknownMetadata(references.trussInfoUuids,
                          references.consumedTrussInfoUuids,
                          "unknown_truss_info_uuid", "TrussInfo", result);
  DiagnoseUnknownMetadata(references.projectFixtureMetadataUuids,
                          references.consumedProjectFixtureMetadataUuids,
                          "unknown_project_fixture_metadata_uuid",
                          "project fixture metadata", result);
  result.fixtureUuidRemap = fixtureUuidRemap_;
}

} // namespace mvr
