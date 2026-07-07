#include "project_truss_gdtf_context.h"

namespace gdtf {

// Builds a non-mutating project-truss editor context from stable truss data.
GdtfEditorContext
BuildProjectTrussGdtfEditorContext(const ProjectTrussGdtfContextInput &input) {
  GdtfEditorContext context;
  context.kind = GdtfEditorContextKind::ProjectTruss;
  context.sourcePath = input.resolvedGdtfPath;
  context.sourceKind = input.sourceKind;
  context.writePolicy = input.writePolicy;
  context.document = input.document;
  context.editingAllowed =
      input.writePolicy == GdtfWritePolicy::ProjectControlledGeneration ||
      input.writePolicy == GdtfWritePolicy::OverwriteOwnedFile;
  context.stableHostId = input.truss.uuid;
  context.hostLabel =
      input.truss.name.empty() ? input.truss.model : input.truss.name;
  context.initialValues.manufacturer = input.truss.manufacturer;
  context.initialValues.modelName = input.truss.model;
  context.initialValues.modeName = input.truss.gdtfMode;
  context.initialValues.weightKg = input.truss.weightKg;
  context.initialValues.trussLengthMm = input.truss.lengthMm;
  context.initialValues.trussWidthMm = input.truss.widthMm;
  context.initialValues.trussHeightMm = input.truss.heightMm;
  context.initialValues.trussCrossSection = input.truss.crossSection;
  context.initialValues.sourceFileReference = input.truss.gdtfSpec.empty()
                                                  ? input.truss.modelFile
                                                  : input.truss.gdtfSpec;
  return context;
}

// Builds a non-GUI edit session for a project truss without generating a GDTF.
GdtfEditSession
BuildProjectTrussGdtfEditSession(const ProjectTrussGdtfContextInput &input) {
  return GdtfEditSession(BuildProjectTrussGdtfEditorContext(input));
}

} // namespace gdtf
