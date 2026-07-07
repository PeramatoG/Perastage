#include "gdtf/editor/gdtf_edit_session.h"
#include "gdtf/editor/project_fixture_gdtf_context.h"
#include "gdtf/editor/project_truss_gdtf_context.h"
#include "gdtf/editor/standalone_gdtf_context.h"

#include <cassert>
#include <filesystem>
#include <string>

namespace {

// Returns a writable fixture context used by session behavior tests.
gdtf::GdtfEditorContext MakeFixtureContext() {
  Fixture fixture;
  fixture.uuid = "fixture-uuid";
  fixture.instanceName = "Fixture A";
  fixture.typeName = "Profile";
  fixture.gdtfSpec = "profile.gdtf";
  fixture.gdtfMode = "Mode 1";
  fixture.weightKg = 20.0f;
  fixture.powerConsumptionW = 500.0f;
  gdtf::ProjectFixtureGdtfContextInput input;
  input.fixture = fixture;
  input.resolvedGdtfPath = "/tmp/profile.gdtf";
  return gdtf::BuildProjectFixtureGdtfEditorContext(input);
}

// Verifies fixture field ownership and value kind are independent concepts.
void AssertFixtureFieldClassification() {
  const auto fields = gdtf::CurrentFixtureEditFieldDescriptors();
  assert(fields.size() == 21);
  const auto *fixtureId =
      gdtf::FindGdtfFieldDescriptor(gdtf::GdtfFieldId::FixtureId);
  assert(fixtureId->ownership ==
         gdtf::GdtfFieldOwnership::MvrProjectInstanceLevel);
  assert(fixtureId->valueKind == gdtf::GdtfFieldValueKind::HostProjectValue);
  assert(fixtureId->hostDialogEditable);
  assert(!fixtureId->sessionValueSupported);
  const auto *mode = gdtf::FindGdtfFieldDescriptor(gdtf::GdtfFieldId::ModeName);
  assert(mode->ownership == gdtf::GdtfFieldOwnership::ContextSpecific);
  assert(mode->valueKind == gdtf::GdtfFieldValueKind::ContextSelection);
  assert(mode->sessionValueSupported);
  assert(fields[8].id == gdtf::GdtfFieldId::ChannelCount);
  assert(fields[8].derived);
  assert(fields[18].ownership ==
         gdtf::GdtfFieldOwnership::ProjectClassificationOverride);
}

// Verifies truss field ownership and value kind match the current edit split.
void AssertTrussFieldClassification() {
  const auto fields = gdtf::CurrentTrussEditFieldDescriptors();
  assert(fields.size() == 18);
  assert(fields[10].id == gdtf::GdtfFieldId::Manufacturer);
  assert(fields[10].ownership == gdtf::GdtfFieldOwnership::GdtfTypeLevel);
  assert(fields[10].valueKind == gdtf::GdtfFieldValueKind::DocumentValue);
  assert(fields[16].id == gdtf::GdtfFieldId::TrussLoad);
  assert(fields[16].valueKind == gdtf::GdtfFieldValueKind::DerivedReadOnly);
  assert(fields[17].id == gdtf::GdtfFieldId::TrussCrossSection);
}

// Verifies unsupported and host-only fields cannot mutate session values.
void AssertRejectedFixtureSessionFields() {
  gdtf::GdtfEditSession session(MakeFixtureContext());
  assert(!session.SetValue(gdtf::GdtfFieldId::Universe, "2"));
  assert(!session.SetValue(gdtf::GdtfFieldId::Layer, "Layer B"));
  assert(!session.SetValue(gdtf::GdtfFieldId::PositionX, "1"));
  assert(!session.SetValue(gdtf::GdtfFieldId::PositionY, "1"));
  assert(!session.SetValue(gdtf::GdtfFieldId::PositionZ, "1"));
  assert(!session.SetValue(gdtf::GdtfFieldId::Roll, "1"));
  assert(!session.SetValue(gdtf::GdtfFieldId::Pitch, "1"));
  assert(!session.SetValue(gdtf::GdtfFieldId::Yaw, "1"));
  assert(!session.SetValue(gdtf::GdtfFieldId::MvrFixtureColor, "#ffffff"));
  assert(!session.SetValue(gdtf::GdtfFieldId::FixtureCategory, "Moving"));
  assert(!session.SetValue(gdtf::GdtfFieldId::VisualColor, "Blue"));
  assert(!session.SetValue(gdtf::GdtfFieldId::ChannelCount, "8"));
  assert(!session.SetValue(gdtf::GdtfFieldId::TrussLoad, "100"));
  assert(!session.IsDirty());
  assert(!gdtf::GetEditableValue(session.CurrentValues(),
                                 gdtf::GdtfFieldId::Universe));
  const auto request = session.BuildApplyRequest();
  assert(request.changedDocumentFields.empty());
  assert(request.changedContextFields.empty());
}

// Verifies dirty tracking follows every successful editable field exactly.
void AssertDirtyTracking() {
  gdtf::GdtfEditSession session(MakeFixtureContext());
  assert(!session.SetValue(gdtf::GdtfFieldId::Weight, "bad"));
  assert(!session.IsDirty());
  assert(session.SetValue(gdtf::GdtfFieldId::Weight, "20"));
  assert(!session.IsDirty());
  assert(session.SetValue(gdtf::GdtfFieldId::Weight, "12.5"));
  assert(session.IsFieldDirty(gdtf::GdtfFieldId::Weight));
  assert(session.SetValue(gdtf::GdtfFieldId::Weight, "20"));
  assert(!session.IsDirty());
  assert(session.SetValue(gdtf::GdtfFieldId::PowerConsumption, "600"));
  assert(session.IsFieldDirty(gdtf::GdtfFieldId::PowerConsumption));
  assert(session.SetValue(gdtf::GdtfFieldId::ModeName, "Mode 2"));
  assert(session.IsFieldDirty(gdtf::GdtfFieldId::ModeName));
  const auto request = session.BuildApplyRequest();
  assert(request.changedDocumentFields.count(gdtf::GdtfFieldId::PowerConsumption));
  assert(request.changedContextFields.count(gdtf::GdtfFieldId::ModeName));
  session.Reset();
  assert(!session.IsDirty());
  const auto resetRequest = session.BuildApplyRequest();
  assert(resetRequest.changedDocumentFields.empty());
  assert(resetRequest.changedContextFields.empty());
}

// Verifies validation only applies to supported active-context values.
void AssertValidationBehavior() {
  gdtf::GdtfEditSession session(MakeFixtureContext());
  assert(session.SetValue(gdtf::GdtfFieldId::Weight, "-1"));
  assert(!session.Validate().empty());
  session.Reset();
  assert(session.SetValue(gdtf::GdtfFieldId::FixtureTypeName, ""));
  assert(!session.Validate().empty());
}

// Verifies project fixture capabilities classify document edits and selections.
void AssertProjectFixtureCapabilities() {
  const auto context = MakeFixtureContext();
  assert(gdtf::FindFieldCapability(context, gdtf::GdtfFieldId::Weight)
             ->operation == gdtf::GdtfFieldEditOperation::DocumentMutation);
  assert(gdtf::FindFieldCapability(context, gdtf::GdtfFieldId::PowerConsumption)
             ->operation == gdtf::GdtfFieldEditOperation::DocumentMutation);
  assert(gdtf::FindFieldCapability(context, gdtf::GdtfFieldId::ModeName)
             ->operation == gdtf::GdtfFieldEditOperation::ContextSelection);
  assert(gdtf::FindFieldCapability(context,
                                   gdtf::GdtfFieldId::SourceFileReference)
             ->operation == gdtf::GdtfFieldEditOperation::ContextSelection);
  assert(!gdtf::FindFieldCapability(context, gdtf::GdtfFieldId::Universe));
}

// Verifies project truss capabilities are limited to generation/type fields.
void AssertProjectTrussCapabilities() {
  Truss truss;
  truss.uuid = "truss-uuid";
  truss.name = "Truss A";
  truss.manufacturer = "Perastage";
  truss.model = "Box";
  truss.gdtfSpec = "truss.gdtf";
  truss.modelFile = "truss.glb";
  truss.lengthMm = 3000.0f;
  truss.widthMm = 290.0f;
  truss.heightMm = 290.0f;
  truss.weightKg = 35.0f;
  truss.crossSection = "box";
  gdtf::ProjectTrussGdtfContextInput input;
  input.truss = truss;
  auto context = gdtf::BuildProjectTrussGdtfEditorContext(input);
  assert(gdtf::FindFieldCapability(context, gdtf::GdtfFieldId::Manufacturer));
  assert(gdtf::FindFieldCapability(context, gdtf::GdtfFieldId::ModelName));
  assert(gdtf::FindFieldCapability(context, gdtf::GdtfFieldId::TrussLength));
  assert(gdtf::FindFieldCapability(context, gdtf::GdtfFieldId::TrussWidth));
  assert(gdtf::FindFieldCapability(context, gdtf::GdtfFieldId::TrussHeight));
  assert(gdtf::FindFieldCapability(context, gdtf::GdtfFieldId::Weight));
  assert(gdtf::FindFieldCapability(context, gdtf::GdtfFieldId::TrussCrossSection));
  assert(!gdtf::FindFieldCapability(context, gdtf::GdtfFieldId::TrussName));
  assert(!gdtf::FindFieldCapability(context, gdtf::GdtfFieldId::PositionX));
  gdtf::GdtfEditSession session(context);
  assert(!session.SetValue(gdtf::GdtfFieldId::TrussName, "Other"));
  assert(session.SetValue(gdtf::GdtfFieldId::TrussLength, "2500"));
}

// Verifies standalone contexts reject edits until an editable write policy is used.
void AssertStandaloneContext() {
  const auto path = std::filesystem::temp_directory_path() /
                    "perastage_missing_standalone_context.gdtf";
  auto readOnly = gdtf::BuildStandaloneGdtfEditorContext(path);
  assert(readOnly.kind == gdtf::GdtfEditorContextKind::StandaloneFile);
  assert(!readOnly.editingAllowed);
  gdtf::GdtfEditSession readOnlySession(readOnly);
  assert(!readOnlySession.SetValue(gdtf::GdtfFieldId::FixtureTypeName, "New"));

  auto editable = gdtf::BuildStandaloneGdtfEditorContext(
      path, gdtf::GdtfWritePolicy::SaveAsNewStandaloneFile);
  gdtf::GdtfEditSession editableSession(editable);
  assert(editableSession.SetValue(gdtf::GdtfFieldId::FixtureTypeName, "New"));
  assert(!editableSession.SetValue(gdtf::GdtfFieldId::ModeName, "Mode 2"));
  assert(!editableSession.SetValue(gdtf::GdtfFieldId::SourceFileReference,
                                   "other.gdtf"));
}

// Verifies project adapters use stable IDs and do not mutate copied scene models.
void AssertProjectAdapters() {
  Fixture fixture;
  fixture.uuid = "fixture-uuid";
  fixture.instanceName = "Fixture A";
  fixture.typeName = "Profile";
  fixture.gdtfSpec = "profile.gdtf";
  gdtf::ProjectFixtureGdtfContextInput fixtureInput;
  fixtureInput.fixture = fixture;
  auto fixtureSession = gdtf::BuildProjectFixtureGdtfEditSession(fixtureInput);
  assert(fixtureSession.Context().stableHostId == "fixture-uuid");
  assert(fixture.gdtfSpec == "profile.gdtf");
}

// Verifies apply results express host side effects independently.
void AssertApplyResultFlags() {
  gdtf::GdtfApplyResult result;
  result.derivativeCreated = true;
  result.viewerRefreshRequired = true;
  result.projectDirtyStateMustBeUpdated = true;
  result.projectInstanceResynchronizationRequired = true;
  assert(result.derivativeCreated);
  assert(result.viewerRefreshRequired);
  assert(result.projectDirtyStateMustBeUpdated);
  assert(result.projectInstanceResynchronizationRequired);
}

// Verifies repeated families are exposed as collections, not singleton fields.
void AssertRepeatedFamilies() {
  gdtf::GdtfDescriptionSnapshot snapshot;
  snapshot.wheels.push_back({"Gobo 1", {}});
  snapshot.wheels.push_back({"Gobo 2", {}});
  gdtf::ArchiveReadResult archive;
  gdtf::GdtfDocument document(std::move(archive), std::move(snapshot));
  assert(document.RepeatedFamilies().size() == 1);
  assert(document.RepeatedFamilies()[0].names.size() == 2);
}

} // namespace

// Runs focused unit checks for the GDTF editor architecture checkpoint.
int main() {
  AssertFixtureFieldClassification();
  AssertTrussFieldClassification();
  AssertRejectedFixtureSessionFields();
  AssertDirtyTracking();
  AssertValidationBehavior();
  AssertProjectFixtureCapabilities();
  AssertProjectTrussCapabilities();
  AssertStandaloneContext();
  AssertProjectAdapters();
  AssertApplyResultFlags();
  AssertRepeatedFamilies();
  return 0;
}
