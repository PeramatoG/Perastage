#include "standalone_gdtf_context.h"

namespace gdtf {

// Builds a standalone-file context through shared read services without project
// dependencies.
GdtfEditorContext
BuildStandaloneGdtfEditorContext(const std::filesystem::path &sourcePath,
                                 GdtfWritePolicy writePolicy) {
  GdtfEditorContext context;
  context.kind = GdtfEditorContextKind::StandaloneFile;
  context.sourcePath = sourcePath;
  context.sourceKind = GdtfSourceKind::StandaloneExternalFile;
  context.writePolicy = writePolicy;
  context.document = LoadGdtfDocument(sourcePath);
  context.editingAllowed =
      writePolicy != GdtfWritePolicy::ReadOnly &&
      writePolicy != GdtfWritePolicy::UnsupportedNotYetAvailable;
  context.stableHostId = sourcePath.generic_string();
  context.hostLabel = sourcePath.filename().generic_string();
  const auto &description = context.document.Description();
  context.initialValues.fixtureTypeName = description.fixtureTypeName;
  context.initialValues.manufacturer = description.manufacturer;
  context.initialValues.modelName = description.shortName.empty()
                                        ? description.longName
                                        : description.shortName;
  if (!description.dmxModeNames.empty())
    context.initialValues.modeName = description.dmxModeNames.front();
  context.initialValues.sourceFileReference =
      sourcePath.filename().generic_string();
  context.fieldCapabilities[GdtfFieldId::FixtureTypeName] =
      MakeDocumentEditCapability();
  context.fieldCapabilities[GdtfFieldId::Manufacturer] =
      MakeDocumentEditCapability();
  context.fieldCapabilities[GdtfFieldId::ModelName] = MakeDocumentEditCapability();
  context.fieldCapabilities[GdtfFieldId::Weight] = MakeDocumentEditCapability();
  context.fieldCapabilities[GdtfFieldId::PowerConsumption] =
      MakeDocumentEditCapability();
  context.fieldCapabilities[GdtfFieldId::ModeName] =
      MakeReadOnlyCapability(GdtfFieldValueKind::ContextSelection);
  context.fieldCapabilities[GdtfFieldId::SourceFileReference] =
      MakeReadOnlyCapability(GdtfFieldValueKind::ContextSelection);
  return context;
}

// Builds a standalone edit session without creating windows or registering file
// associations.
GdtfEditSession
BuildStandaloneGdtfEditSession(const std::filesystem::path &sourcePath,
                               GdtfWritePolicy writePolicy) {
  return GdtfEditSession(
      BuildStandaloneGdtfEditorContext(sourcePath, writePolicy));
}

} // namespace gdtf
