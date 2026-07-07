#include "gdtf/editor/gdtf_edit_session.h"
#include "gdtf/editor/project_fixture_gdtf_context.h"
#include "gdtf/editor/project_truss_gdtf_context.h"
#include "gdtf/editor/standalone_gdtf_context.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

// Writes a minimal GDTF-like archive placeholder for missing-file independent
// tests.
void AssertFixtureFieldOwnership() {
  const auto fields = gdtf::CurrentFixtureEditFieldDescriptors();
  assert(fields.size() == 21);
  assert(fields[2].id == gdtf::GdtfFieldId::FixtureTypeName);
  assert(fields[2].ownership == gdtf::GdtfFieldOwnership::GdtfTypeLevel);
  assert(fields[8].id == gdtf::GdtfFieldId::ChannelCount);
  assert(fields[8].ownership == gdtf::GdtfFieldOwnership::DerivedReadOnly);
  assert(fields[18].ownership ==
         gdtf::GdtfFieldOwnership::ProjectClassificationOverride);
}

// Verifies truss field ownership matches the current Edit Truss split.
void AssertTrussFieldOwnership() {
  const auto fields = gdtf::CurrentTrussEditFieldDescriptors();
  assert(fields.size() == 18);
  assert(fields[10].id == gdtf::GdtfFieldId::Manufacturer);
  assert(fields[10].ownership == gdtf::GdtfFieldOwnership::GdtfTypeLevel);
  assert(fields[16].id == gdtf::GdtfFieldId::TrussLoad);
  assert(fields[16].ownership == gdtf::GdtfFieldOwnership::DerivedReadOnly);
  assert(fields[17].id == gdtf::GdtfFieldId::TrussCrossSection);
}

// Verifies dirty tracking, validation, reset, and derived-field protection.
void AssertSessionBehavior() {
  gdtf::GdtfEditorContext context;
  context.editingAllowed = true;
  context.initialValues.fixtureTypeName = "Spot";
  context.initialValues.weightKg = 10.0f;
  gdtf::GdtfEditSession session(context);
  assert(!session.IsDirty());
  assert(!session.SetValue(gdtf::GdtfFieldId::ChannelCount, "8"));
  assert(!session.IsDirty());
  assert(session.SetValue(gdtf::GdtfFieldId::Weight, "12.5"));
  assert(session.IsDirty());
  assert(session.IsFieldDirty(gdtf::GdtfFieldId::Weight));
  assert(session.DirtyFields().size() == 1);
  assert(session.Validate().empty());
  assert(session.SetValue(gdtf::GdtfFieldId::Weight, "-1"));
  assert(!session.Validate().empty());
  session.Reset();
  assert(!session.IsDirty());
}

// Verifies source kind and write policy are separate independent values.
void AssertPolicySeparation() {
  gdtf::GdtfEditorContext context;
  context.sourceKind = gdtf::GdtfSourceKind::EmbeddedOrExtractedFromMvr;
  context.writePolicy = gdtf::GdtfWritePolicy::CreateDerivativeBeforeMutation;
  assert(std::string(gdtf::ToString(context.sourceKind)).find("MVR") !=
         std::string::npos);
  assert(std::string(gdtf::ToString(context.writePolicy)).find("derivative") !=
         std::string::npos);
}

// Verifies project adapters use stable IDs and do not mutate copied scene
// models.
void AssertProjectAdapters() {
  Fixture fixture;
  fixture.uuid = "fixture-uuid";
  fixture.instanceName = "Fixture A";
  fixture.typeName = "Profile";
  fixture.gdtfSpec = "profile.gdtf";
  fixture.gdtfMode = "Mode 1";
  fixture.weightKg = 20.0f;
  fixture.powerConsumptionW = 500.0f;
  gdtf::ProjectFixtureGdtfContextInput fixtureInput;
  fixtureInput.fixture = fixture;
  fixtureInput.resolvedGdtfPath = "/tmp/profile.gdtf";
  auto fixtureSession = gdtf::BuildProjectFixtureGdtfEditSession(fixtureInput);
  assert(fixtureSession.Context().stableHostId == "fixture-uuid");
  assert(fixture.gdtfSpec == "profile.gdtf");

  Truss truss;
  truss.uuid = "truss-uuid";
  truss.name = "Truss A";
  truss.manufacturer = "Perastage";
  truss.model = "Box";
  truss.gdtfSpec = "truss.gdtf";
  truss.lengthMm = 3000.0f;
  truss.widthMm = 290.0f;
  truss.heightMm = 290.0f;
  truss.weightKg = 35.0f;
  truss.crossSection = "box";
  gdtf::ProjectTrussGdtfContextInput trussInput;
  trussInput.truss = truss;
  auto trussSession = gdtf::BuildProjectTrussGdtfEditSession(trussInput);
  assert(trussSession.Context().stableHostId == "truss-uuid");
  assert(truss.gdtfSpec == "truss.gdtf");
}

// Verifies apply results express host side effects independently.
void AssertApplyResultFlags() {
  gdtf::GdtfApplyResult result;
  result.derivativeCreated = true;
  result.viewerRefreshRequired = true;
  result.projectDirtyStateMustBeUpdated = true;
  result.hoistLoadRecalculationRequired = false;
  result.projectInstanceResynchronizationRequired = true;
  assert(result.derivativeCreated);
  assert(result.viewerRefreshRequired);
  assert(result.projectDirtyStateMustBeUpdated);
  assert(result.projectInstanceResynchronizationRequired);
}

// Verifies repeated families are exposed as collections, not hardcoded
// singleton fields.
void AssertRepeatedFamilies() {
  gdtf::GdtfDescriptionSnapshot snapshot;
  snapshot.wheels.push_back({"Gobo 1", {}});
  snapshot.wheels.push_back({"Gobo 2", {}});
  gdtf::ArchiveReadResult archive;
  gdtf::GdtfDocument document(std::move(archive), std::move(snapshot));
  assert(document.RepeatedFamilies().size() == 1);
  assert(document.RepeatedFamilies()[0].names.size() == 2);
}

// Verifies the standalone context remains project-independent and read-only by
// default.
void AssertStandaloneContext() {
  const auto path = std::filesystem::temp_directory_path() /
                    "perastage_missing_standalone_context.gdtf";
  auto context = gdtf::BuildStandaloneGdtfEditorContext(path);
  assert(context.kind == gdtf::GdtfEditorContextKind::StandaloneFile);
  assert(context.writePolicy == gdtf::GdtfWritePolicy::ReadOnly);
  assert(!context.editingAllowed);
  assert(context.stableHostId.find("perastage_missing_standalone_context") !=
         std::string::npos);
}

} // namespace

// Runs focused unit checks for the GDTF editor architecture checkpoint.
int main() {
  AssertFixtureFieldOwnership();
  AssertTrussFieldOwnership();
  AssertSessionBehavior();
  AssertPolicySeparation();
  AssertProjectAdapters();
  AssertApplyResultFlags();
  AssertRepeatedFamilies();
  AssertStandaloneContext();
  return 0;
}
