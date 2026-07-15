#include "project_fixture_gdtf_context.h"

namespace gdtf {

// Builds a non-mutating project-fixture editor context from stable fixture
// data.
GdtfEditorContext BuildProjectFixtureGdtfEditorContext(
    const ProjectFixtureGdtfContextInput &input) {
  GdtfEditorContext context;
  context.kind = GdtfEditorContextKind::ProjectFixture;
  context.sourcePath = input.resolvedGdtfPath;
  context.sourceKind = input.sourceKind;
  context.writePolicy = input.writePolicy;
  context.document = input.document;
  context.editingAllowed =
      input.writePolicy != GdtfWritePolicy::ReadOnly &&
      input.writePolicy != GdtfWritePolicy::UnsupportedNotYetAvailable;
  context.stableHostId = input.fixture.uuid;
  context.hostLabel = input.fixture.instanceName.empty()
                          ? input.fixture.typeName
                          : input.fixture.instanceName;
  context.initialValues.fixtureTypeName = input.fixture.typeName;
  context.initialValues.modeName = input.fixture.gdtfMode;
  context.initialValues.fixtureTypeDescription = input.document.Description().description;
  context.initialValues.weightKg = input.fixture.weightKg;
  context.initialValues.powerConsumptionW = input.fixture.powerConsumptionW;
  context.initialValues.sourceFileReference =
      !input.editorSourceFileReference.empty()
          ? input.editorSourceFileReference
          : (!input.resolvedGdtfPath.empty() ? input.resolvedGdtfPath.string()
                                             : input.fixture.gdtfSpec);
  context.fieldCapabilities[GdtfFieldId::FixtureTypeName] =
      MakeContextSelectionCapability();
  context.fieldCapabilities[GdtfFieldId::ModeName] =
      MakeContextSelectionCapability();
  context.fieldCapabilities[GdtfFieldId::SourceFileReference] =
      MakeContextSelectionCapability();
  context.fieldCapabilities[GdtfFieldId::FixtureTypeDescription] =
      MakeDocumentEditCapability();
  context.fieldCapabilities[GdtfFieldId::Weight] = MakeDocumentEditCapability();
  context.fieldCapabilities[GdtfFieldId::PowerConsumption] =
      MakeDocumentEditCapability();
  context.fieldCapabilities[GdtfFieldId::ChannelCount] =
      MakeReadOnlyCapability(GdtfFieldValueKind::DerivedReadOnly);
  return context;
}

// Builds a non-GUI edit session for a project fixture without mutating scene
// state.
GdtfEditSession BuildProjectFixtureGdtfEditSession(
    const ProjectFixtureGdtfContextInput &input) {
  return GdtfEditSession(BuildProjectFixtureGdtfEditorContext(input));
}

} // namespace gdtf
