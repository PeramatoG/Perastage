#include "gdtf/editor/gdtf_edit_session.h"
#include "gdtf/editor/project_fixture_gdtf_context.h"
#include "gdtf/editor/project_truss_gdtf_context.h"
#include "gdtf/editor/standalone_gdtf_context.h"

#include <cassert>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

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
  input.editorSourceFileReference = "/tmp/profile.gdtf";
  return gdtf::BuildProjectFixtureGdtfEditorContext(input);
}

// Verifies fixture field ownership and value kind are independent concepts.
void AssertFixtureFieldClassification() {
  const auto fields = gdtf::CurrentFixtureEditFieldDescriptors();
  assert(fields.size() == 22);
  const auto *fixtureId =
      gdtf::FindGdtfFieldDescriptor(gdtf::GdtfFieldId::FixtureId);
  assert(fixtureId->ownership ==
         gdtf::GdtfFieldOwnership::MvrProjectInstanceLevel);
  assert(fixtureId->defaultValueKind == gdtf::GdtfFieldValueKind::HostProjectValue);
  assert(fixtureId->hostDialogEditable);
  assert(!fixtureId->sessionValueSupported);
  const auto *mode = gdtf::FindGdtfFieldDescriptor(gdtf::GdtfFieldId::ModeName);
  assert(mode->ownership == gdtf::GdtfFieldOwnership::ContextSpecific);
  assert(mode->defaultValueKind == gdtf::GdtfFieldValueKind::ContextSelection);
  assert(mode->sessionValueSupported);
  assert(fields[8].id == gdtf::GdtfFieldId::ChannelCount);
  assert(fields[8].derived);
  assert(fields[19].ownership ==
         gdtf::GdtfFieldOwnership::ProjectClassificationOverride);
}

// Verifies truss field ownership and value kind match the current edit split.
void AssertTrussFieldClassification() {
  const auto fields = gdtf::CurrentTrussEditFieldDescriptors();
  assert(fields.size() == 20);
  assert(fields[10].id == gdtf::GdtfFieldId::Manufacturer);
  assert(fields[10].ownership == gdtf::GdtfFieldOwnership::GdtfTypeLevel);
  assert(fields[10].defaultValueKind == gdtf::GdtfFieldValueKind::DocumentValue);
  assert(fields[16].id == gdtf::GdtfFieldId::FixtureTypeDescription);
  assert(fields[17].id == gdtf::GdtfFieldId::TrussLoad);
  assert(fields[17].defaultValueKind == gdtf::GdtfFieldValueKind::DerivedReadOnly);
  assert(fields[18].id == gdtf::GdtfFieldId::TrussCrossSectionType);
  assert(fields[19].id == gdtf::GdtfFieldId::TrussCrossSection);
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

// Verifies editable numeric fields parse portable finite floating-point text.
void AssertEditableNumericParsing() {
  const std::vector<std::string> validValues = {"0", "12", "12.5", "-0.25",
                                                "1e3", "1.25e-2"};
  for (const auto &value : validValues) {
    gdtf::GdtfEditableValues values;
    assert(gdtf::SetEditableValue(values, gdtf::GdtfFieldId::Weight, value));
    assert(gdtf::GetEditableValue(values, gdtf::GdtfFieldId::Weight));
  }

  const std::vector<std::string> invalidValues = {"",     " ",   " 12",
                                                  "12 ", "12abc", "12,5",
                                                  "nan", "NaN",  "inf",
                                                  "-inf"};
  for (const auto &value : invalidValues) {
    gdtf::GdtfEditableValues values;
    assert(gdtf::SetEditableValue(values, gdtf::GdtfFieldId::Weight, "12.5"));
    const auto before = gdtf::GetEditableValue(values, gdtf::GdtfFieldId::Weight);
    assert(!gdtf::SetEditableValue(values, gdtf::GdtfFieldId::Weight, value));
    assert(gdtf::GetEditableValue(values, gdtf::GdtfFieldId::Weight) == before);
  }

  gdtf::GdtfEditableValues supported;
  assert(gdtf::SetEditableValue(supported, gdtf::GdtfFieldId::Weight, "12.5"));
  assert(gdtf::SetEditableValue(supported, gdtf::GdtfFieldId::PowerConsumption,
                                "12.5"));
  assert(gdtf::SetEditableValue(supported, gdtf::GdtfFieldId::TrussLength,
                                "12.5"));
  assert(gdtf::SetEditableValue(supported, gdtf::GdtfFieldId::TrussWidth,
                                "12.5"));
  assert(gdtf::SetEditableValue(supported, gdtf::GdtfFieldId::TrussHeight,
                                "12.5"));
  assert(gdtf::SetEditableValue(supported, gdtf::GdtfFieldId::FixtureTypeName,
                                "12,5"));
  assert(gdtf::SetEditableValue(supported,
                                gdtf::GdtfFieldId::FixtureTypeDescription,
                                "Line 1\nŁine 2"));
  assert(gdtf::SetEditableValue(supported,
                                gdtf::GdtfFieldId::TrussCrossSectionType,
                                "TrussFramework"));
  assert(gdtf::SetEditableValue(supported,
                                gdtf::GdtfFieldId::TrussCrossSectionType,
                                "Tube"));
  assert(!gdtf::SetEditableValue(supported,
                                 gdtf::GdtfFieldId::TrussCrossSectionType,
                                 "Round"));
  assert(gdtf::GetEditableValue(supported, gdtf::GdtfFieldId::FixtureTypeName) ==
         std::optional<std::string>("12,5"));
}

// Verifies invalid numeric edits preserve session state and dirty tracking.
void AssertEditableNumericSessionSemantics() {
  gdtf::GdtfEditSession session(MakeFixtureContext());
  assert(session.SetValue(gdtf::GdtfFieldId::Weight, "12.5"));
  assert(session.IsFieldDirty(gdtf::GdtfFieldId::Weight));
  const auto before = gdtf::GetEditableValue(session.CurrentValues(),
                                             gdtf::GdtfFieldId::Weight);
  assert(!session.SetValue(gdtf::GdtfFieldId::Weight, "12abc"));
  assert(gdtf::GetEditableValue(session.CurrentValues(),
                                gdtf::GdtfFieldId::Weight) == before);
  assert(session.IsFieldDirty(gdtf::GdtfFieldId::Weight));

  const auto dirty = session.CurrentValues();
  auto equal = dirty;
  assert(equal == dirty);
  assert(!(equal != dirty));
}

// Verifies fixture sessions classify all checkpoint 08A supported fields.
void AssertFixtureSessionCheckpoint08A() {
  auto context = MakeFixtureContext();
  gdtf::GdtfEditSession editable(context);
  assert(editable.Context().stableHostId == "fixture-uuid");
  assert(!editable.IsDirty());
  assert(editable.SetValue(gdtf::GdtfFieldId::FixtureTypeName, "Wash"));
  assert(editable.IsFieldDirty(gdtf::GdtfFieldId::FixtureTypeName));
  assert(editable.BuildApplyRequest().changedContextFields.count(
      gdtf::GdtfFieldId::FixtureTypeName));
  assert(editable.SetValue(gdtf::GdtfFieldId::SourceFileReference,
                           "other.gdtf"));
  assert(editable.IsFieldDirty(gdtf::GdtfFieldId::SourceFileReference));
  assert(editable.SetValue(gdtf::GdtfFieldId::ModeName, "Mode 2"));
  assert(editable.IsFieldDirty(gdtf::GdtfFieldId::ModeName));
  assert(editable.SetValue(gdtf::GdtfFieldId::PowerConsumption, "650"));
  assert(editable.BuildApplyRequest().changedDocumentFields.count(
      gdtf::GdtfFieldId::PowerConsumption));
  assert(!editable.SetValue(gdtf::GdtfFieldId::Weight, "bad"));
  assert(editable.SetValue(gdtf::GdtfFieldId::Weight, "-2"));
  assert(!editable.Validate().empty());
  assert(editable.SetValue(gdtf::GdtfFieldId::Weight, "20"));
  assert(!editable.IsFieldDirty(gdtf::GdtfFieldId::Weight));
}

// Verifies truss sessions classify checkpoint 08A type-generation fields.
void AssertTrussSessionCheckpoint08A() {
  Truss truss;
  truss.uuid = "truss-uuid";
  truss.manufacturer = "A";
  truss.model = "B";
  truss.lengthMm = 1000.0f;
  truss.widthMm = 200.0f;
  truss.heightMm = 300.0f;
  truss.weightKg = 40.0f;
  truss.crossSection = "box";
  truss.gdtfSpec = "truss.gdtf";
  gdtf::ProjectTrussGdtfContextInput input;
  input.truss = truss;
  auto session = gdtf::BuildProjectTrussGdtfEditSession(input);
  assert(session.Context().stableHostId == "truss-uuid");
  assert(!session.IsDirty());
  assert(session.SetValue(gdtf::GdtfFieldId::Manufacturer, "C"));
  assert(session.SetValue(gdtf::GdtfFieldId::ModelName, "D"));
  assert(session.SetValue(gdtf::GdtfFieldId::TrussLength, "1100"));
  assert(session.SetValue(gdtf::GdtfFieldId::TrussWidth, "220"));
  assert(session.SetValue(gdtf::GdtfFieldId::TrussHeight, "330"));
  assert(session.SetValue(gdtf::GdtfFieldId::Weight, "45"));
  assert(session.SetValue(gdtf::GdtfFieldId::FixtureTypeDescription,
                          "Multiline\nDescription"));
  assert(session.SetValue(gdtf::GdtfFieldId::TrussCrossSectionType, "Tube"));
  assert(!session.SetValue(gdtf::GdtfFieldId::TrussCrossSectionType, "Round"));
  assert(session.SetValue(gdtf::GdtfFieldId::TrussCrossSection, "triangle"));
  assert(session.BuildApplyRequest().changedDocumentFields.count(
      gdtf::GdtfFieldId::TrussCrossSection));
  assert(session.BuildApplyRequest().changedDocumentFields.count(
      gdtf::GdtfFieldId::TrussCrossSectionType));
  assert(session.BuildApplyRequest().changedDocumentFields.count(
      gdtf::GdtfFieldId::FixtureTypeDescription));
  assert(session.SetValue(gdtf::GdtfFieldId::SourceFileReference,
                          "new-truss.gdtf"));
  assert(session.BuildApplyRequest().changedContextFields.count(
      gdtf::GdtfFieldId::SourceFileReference));
  assert(!session.SetValue(gdtf::GdtfFieldId::TrussName, "MVR only"));
  assert(session.SetValue(gdtf::GdtfFieldId::TrussLength, "-1"));
  assert(!session.Validate().empty());
  assert(session.SetValue(gdtf::GdtfFieldId::TrussLength, "1000"));
  assert(!session.IsFieldDirty(gdtf::GdtfFieldId::TrussLength));
}

// Verifies source rebinding replaces document context without changing baseline.
void AssertSourceRebindingCheckpoint08A() {
  gdtf::GdtfEditSession session(MakeFixtureContext());
  assert(session.SetValue(gdtf::GdtfFieldId::PowerConsumption, "700"));
  auto rebound = MakeFixtureContext();
  rebound.sourcePath = "/tmp/other.gdtf";
  session.RebindContextPreservingValues(std::move(rebound));
  assert(session.Context().sourcePath == std::filesystem::path("/tmp/other.gdtf"));
  assert(session.IsFieldDirty(gdtf::GdtfFieldId::PowerConsumption));
  session.AcceptCurrentValues();
  assert(!session.IsDirty());
  assert(session.SetValue(gdtf::GdtfFieldId::PowerConsumption, "800"));
  assert(session.IsFieldDirty(gdtf::GdtfFieldId::PowerConsumption));
  session.Reset();
  assert(!session.IsDirty());
}

// Verifies resolved fixture source presentation does not use a portable reference.
void AssertFixtureResolvedSourcePresentation() {
  Fixture fixture;
  fixture.uuid = "fixture-relative";
  fixture.typeName = "Profile";
  fixture.gdtfSpec = "Fixture.gdtf";
  gdtf::ProjectFixtureGdtfContextInput input;
  input.fixture = fixture;
  input.resolvedGdtfPath = "/tmp/scene/Fixture.gdtf";
  input.editorSourceFileReference = input.resolvedGdtfPath.string();
  auto session = gdtf::BuildProjectFixtureGdtfEditSession(input);
  assert(session.Context().sourcePath ==
         std::filesystem::path("/tmp/scene/Fixture.gdtf"));
  assert(session.CurrentValues().sourceFileReference);
  assert(*session.CurrentValues().sourceFileReference ==
         "/tmp/scene/Fixture.gdtf");
  assert(*session.CurrentValues().sourceFileReference != fixture.gdtfSpec);
  assert(!session.IsDirty());
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

// Verifies operation and effective value kind are internally consistent.
void AssertCapabilityInvariant(const gdtf::GdtfFieldCapability &capability) {
  if (capability.operation == gdtf::GdtfFieldEditOperation::DocumentMutation)
    assert(capability.valueKind == gdtf::GdtfFieldValueKind::DocumentValue);
  if (capability.operation == gdtf::GdtfFieldEditOperation::ContextSelection)
    assert(capability.valueKind == gdtf::GdtfFieldValueKind::ContextSelection);
  if (capability.operation == gdtf::GdtfFieldEditOperation::ReadOnly)
    assert(!capability.editable);
}

// Verifies project fixture capabilities classify document edits and selections.
void AssertProjectFixtureCapabilities() {
  const auto context = MakeFixtureContext();
  for (const auto &[fieldId, capability] : context.fieldCapabilities)
    AssertCapabilityInvariant(capability);
  assert(gdtf::FindFieldCapability(context, gdtf::GdtfFieldId::Weight)
             ->operation == gdtf::GdtfFieldEditOperation::DocumentMutation);
  assert(gdtf::FindFieldCapability(context, gdtf::GdtfFieldId::PowerConsumption)
             ->operation == gdtf::GdtfFieldEditOperation::DocumentMutation);
  assert(gdtf::FindFieldCapability(context, gdtf::GdtfFieldId::FixtureTypeName)
             ->operation == gdtf::GdtfFieldEditOperation::ContextSelection);
  assert(gdtf::FindFieldCapability(context, gdtf::GdtfFieldId::FixtureTypeName)
             ->valueKind == gdtf::GdtfFieldValueKind::ContextSelection);
  assert(gdtf::FindFieldCapability(context, gdtf::GdtfFieldId::ModeName)
             ->operation == gdtf::GdtfFieldEditOperation::ContextSelection);
  assert(gdtf::FindFieldCapability(context, gdtf::GdtfFieldId::ModeName)
             ->valueKind == gdtf::GdtfFieldValueKind::ContextSelection);
  assert(gdtf::FindFieldCapability(context,
                                   gdtf::GdtfFieldId::SourceFileReference)
             ->operation == gdtf::GdtfFieldEditOperation::ContextSelection);
  assert(gdtf::FindFieldCapability(context,
                                   gdtf::GdtfFieldId::SourceFileReference)
             ->valueKind == gdtf::GdtfFieldValueKind::ContextSelection);
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
  const auto *readOnlyType =
      gdtf::FindFieldCapability(readOnly, gdtf::GdtfFieldId::FixtureTypeName);
  assert(readOnlyType);
  assert(readOnlyType->operation == gdtf::GdtfFieldEditOperation::ReadOnly);
  assert(readOnlyType->valueKind == gdtf::GdtfFieldValueKind::DocumentValue);
  gdtf::GdtfEditSession readOnlySession(readOnly);
  assert(!readOnlySession.SetValue(gdtf::GdtfFieldId::FixtureTypeName, "New"));

  auto editable = gdtf::BuildStandaloneGdtfEditorContext(
      path, gdtf::GdtfWritePolicy::SaveAsNewStandaloneFile);
  const auto *editableType =
      gdtf::FindFieldCapability(editable, gdtf::GdtfFieldId::FixtureTypeName);
  assert(editableType);
  assert(editableType->operation == gdtf::GdtfFieldEditOperation::DocumentMutation);
  assert(editableType->valueKind == gdtf::GdtfFieldValueKind::DocumentValue);
  for (const auto &[fieldId, capability] : editable.fieldCapabilities)
    AssertCapabilityInvariant(capability);
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
  AssertEditableNumericParsing();
  AssertEditableNumericSessionSemantics();
  AssertFixtureSessionCheckpoint08A();
  AssertTrussSessionCheckpoint08A();
  AssertSourceRebindingCheckpoint08A();
  AssertFixtureResolvedSourcePresentation();
  AssertValidationBehavior();
  AssertProjectFixtureCapabilities();
  AssertProjectTrussCapabilities();
  AssertStandaloneContext();
  AssertProjectAdapters();
  AssertApplyResultFlags();
  AssertRepeatedFamilies();
  return 0;
}
