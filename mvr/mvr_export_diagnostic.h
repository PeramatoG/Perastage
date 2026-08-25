#pragma once

#include <string>

// Stable machine-readable classifications emitted by MVR export.
enum class MvrExportDiagnosticCode {
  IdentityCanonicalized,
  IdentityGenerated,
  IdentityReassigned,
  IdentityConflict,
  ReferenceCleared,
  LayerInferred,
  TransformRepaired,
  TransformInvalid,
  SymbolIdentityReplaced,
  FixtureIdReassigned,
  GdtfFallbackUsed,
  GdtfMissing,
  TrussGdtfMissing,
  GdtfPatchFailed,
  TextureMissing,
  ResourceMissing,
  ResourceDuplicate,
  SupportGeometryMissing,
  PlaceholderGeometryUsed,
  CompatibilityRepresentationUnavailable,
  ForeignMetadataDiscarded,
  DmxAddressOmitted,
  StructuralValidationFailed,
  CanonicalizationFailed,
  ArchiveIoFailed,
  HierarchyRecursion,
  InternalRecovery
};

enum class MvrExportDiagnosticSeverity { Info, Warning, Error };
enum class MvrExportDiagnosticImpact { None, IdentityChanged, DataOmitted,
                                       DataSubstituted, RequestNotHonored,
                                       ExportFailed };

// Carries exporter diagnostics without dependencies on a presentation toolkit.
struct MvrExportDiagnostic {
  MvrExportDiagnosticCode code = MvrExportDiagnosticCode::InternalRecovery;
  MvrExportDiagnosticSeverity severity = MvrExportDiagnosticSeverity::Info;
  MvrExportDiagnosticImpact impact = MvrExportDiagnosticImpact::None;
  bool userVisible = false;
  std::string objectType;
  std::string objectName;
  std::string objectIdentity;
  std::string resourceName;
  std::string technicalDetail;
};
