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

} // namespace gdtf
