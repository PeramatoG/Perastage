#include "mvr_import_reference_resolver.h"

#include "uuidutils.h"

#include <cassert>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

constexpr const char *kFixtureUuid = "20000000-0000-4000-8000-000000000001";

// Builds a repeatable imported identity context for resolver tests.
mvr::MvrImportedIdentity Identity(const std::string &rawUuid) {
  return {"Fixture", "Layer", "Fixture 1", "1,0,0,0,1,0,0,0,1,0,0,0",
          rawUuid,   {}};
}

// Verifies canonical identities are preserved and legacy recovery is stable.
void TestStableIdentities() {
  mvr::MvrImportReferenceResolver first;
  assert(first.ResolveStableUuid(Identity(kFixtureUuid)) == kFixtureUuid);

  const std::string repaired = first.ResolveStableUuid(Identity("legacy-id"));
  mvr::MvrImportReferenceResolver second;
  assert(second.ResolveStableUuid(Identity("legacy-id")) == repaired);
  assert(CanonicalizeUuid(repaired) == repaired);

  const std::string collision = first.ResolveStableUuid(Identity(kFixtureUuid));
  assert(collision != kFixtureUuid);
  mvr::MvrImportReferenceResolver repeat;
  repeat.ResolveStableUuid(Identity(kFixtureUuid));
  assert(repeat.ResolveStableUuid(Identity(kFixtureUuid)) == collision);
}

// Verifies standard and legacy Position identities retain their import
// behavior.
void TestPositionNormalization() {
  mvr::MvrImportReferenceResolver resolver;
  MvrScene scene;
  resolver.ImportPosition(kFixtureUuid, "  FOH Position  ", scene);
  assert(scene.positions.at(kFixtureUuid) == "  FOH Position  ");

  resolver.ImportPosition("legacy-position", "  Legacy Position  ", scene);
  const std::string recovered =
      resolver.LegacyPositionRemap().at("legacy-position");
  assert(recovered ==
         DeriveDeterministicUuid(
             "mvr:legacy-position:legacy-position:Legacy Position"));
  assert(scene.positions.at(recovered) == "  Legacy Position  ");
  assert(resolver.EnsurePosition("legacy-position", scene) ==
         "  Legacy Position  ");

  resolver.ImportPosition("whitespace-position", "   \t  ", scene);
  const std::string whitespaceUuid =
      DeriveDeterministicUuid("mvr:legacy-position:whitespace-position:");
  assert(scene.positions.at(whitespaceUuid) == "   \t  ");

  resolver.ImportPosition("missing-name-position", std::nullopt, scene);
  const std::string missingNameUuid =
      DeriveDeterministicUuid("mvr:legacy-position:missing-name-position:");
  assert(scene.positions.at(missingNameUuid) == "missing-name-position");

  resolver.ImportPosition("empty-name-position", "", scene);
  const std::string emptyNameUuid =
      DeriveDeterministicUuid("mvr:legacy-position:empty-name-position:");
  assert(scene.positions.at(emptyNameUuid).empty());
}

// Verifies complete-scene aliases, validation, diagnostics, and ordering.
void TestPostParseReconciliation() {
  mvr::MvrImportReferenceResolver resolver;
  const std::string fixtureAlias = "20000000000040008000000000000001";
  resolver.RecordFixtureUuid(fixtureAlias, kFixtureUuid);

  MvrImportResult result;
  Fixture fixture;
  fixture.uuid = kFixtureUuid;
  result.scene.fixtures.emplace(fixture.uuid, fixture);
  Support valid;
  valid.uuid = "10000000-0000-4000-8000-000000000001";
  valid.motorFixtureUuid = fixtureAlias;
  result.scene.supports.emplace(valid.uuid, valid);
  Support unknown;
  unknown.uuid = "10000000-0000-4000-8000-000000000002";
  unknown.motorFixtureUuid = "20000000-0000-4000-8000-000000000099";
  result.scene.supports.emplace(unknown.uuid, unknown);

  const std::unordered_set<std::string> truss = {"truss-b", "truss-a"};
  const std::unordered_set<std::string> hoist = {"hoist-b", "hoist-a"};
  const std::unordered_set<std::string> project = {"project-b", "project-a"};
  const std::unordered_set<std::string> consumed;
  resolver.Reconcile(result,
                     {truss, consumed, hoist, consumed, project, consumed});

  assert(result.scene.supports.at(valid.uuid).motorFixtureUuid == kFixtureUuid);
  assert(result.scene.supports.at(unknown.uuid).motorFixtureUuid.empty());
  assert(result.fixtureUuidRemap.at(fixtureAlias) == kFixtureUuid);
  assert(result.diagnostics.size() == 7);
  assert(result.diagnostics[0].code == "unknown_motor_fixture_uuid");
  assert(result.diagnostics[1].message.find("hoist-a") != std::string::npos);
  assert(result.diagnostics[2].message.find("hoist-b") != std::string::npos);
  assert(result.diagnostics[3].message.find("truss-a") != std::string::npos);
  assert(result.diagnostics[4].message.find("truss-b") != std::string::npos);
  assert(result.diagnostics[5].message.find("project-a") != std::string::npos);
  assert(result.diagnostics[6].message.find("project-b") != std::string::npos);
}

} // namespace

// Runs the focused CH-104 reference normalization characterization tests.
int main() {
  TestStableIdentities();
  TestPositionNormalization();
  TestPostParseReconciliation();
  return 0;
}
