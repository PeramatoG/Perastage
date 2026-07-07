#include "gdtf_editor_context.h"

namespace gdtf {

// Converts a source-kind enum to a stable diagnostic string.
const char *ToString(GdtfSourceKind kind) {
  switch (kind) {
  case GdtfSourceKind::StandaloneExternalFile:
    return "standalone external file";
  case GdtfSourceKind::PerastageFixtureLibraryFile:
    return "Perastage fixture library file";
  case GdtfSourceKind::PerastageTrussLibraryFile:
    return "Perastage truss library file";
  case GdtfSourceKind::EmbeddedOrExtractedFromMvr:
    return "GDTF embedded/extracted from MVR";
  case GdtfSourceKind::PerastageGeneratedDerivative:
    return "Perastage-generated derivative";
  case GdtfSourceKind::TemporaryGeneratedFile:
    return "temporary/generated file";
  case GdtfSourceKind::Unknown:
    return "unknown source";
  }
  return "unknown source";
}

// Converts a write-policy enum to a stable diagnostic string.
const char *ToString(GdtfWritePolicy policy) {
  switch (policy) {
  case GdtfWritePolicy::ReadOnly:
    return "read-only";
  case GdtfWritePolicy::OverwriteOwnedFile:
    return "overwrite owned file";
  case GdtfWritePolicy::CreateDerivativeBeforeMutation:
    return "create derivative before mutation";
  case GdtfWritePolicy::SaveAsNewStandaloneFile:
    return "save as new standalone file";
  case GdtfWritePolicy::ProjectControlledGeneration:
    return "project-controlled generation";
  case GdtfWritePolicy::UnsupportedNotYetAvailable:
    return "unsupported/not yet available";
  }
  return "unsupported/not yet available";
}

// Finds a field capability declared by the active editor context.
const GdtfFieldCapability *FindFieldCapability(const GdtfEditorContext &context,
                                               GdtfFieldId fieldId) {
  if (auto it = context.fieldCapabilities.find(fieldId);
      it != context.fieldCapabilities.end())
    return &it->second;
  return nullptr;
}

// Creates a visible read-only capability for inspection-only fields.
GdtfFieldCapability MakeReadOnlyCapability(GdtfFieldValueKind valueKind) {
  return {true, false, valueKind, GdtfFieldEditOperation::ReadOnly};
}

// Creates an editable capability for GDTF document/type mutations.
GdtfFieldCapability MakeDocumentEditCapability() {
  return {true, true, GdtfFieldValueKind::DocumentValue,
          GdtfFieldEditOperation::DocumentMutation};
}

// Creates an editable capability for host/context selections.
GdtfFieldCapability MakeContextSelectionCapability() {
  return {true, true, GdtfFieldValueKind::ContextSelection,
          GdtfFieldEditOperation::ContextSelection};
}

} // namespace gdtf
