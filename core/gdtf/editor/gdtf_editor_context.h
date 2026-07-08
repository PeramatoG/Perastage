#pragma once

#include "gdtf_document.h"
#include "gdtf_editable_values.h"

#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace gdtf {

enum class GdtfSourceKind {
  StandaloneExternalFile,
  PerastageFixtureLibraryFile,
  PerastageTrussLibraryFile,
  EmbeddedOrExtractedFromMvr,
  PerastageGeneratedDerivative,
  TemporaryGeneratedFile,
  Unknown
};

enum class GdtfWritePolicy {
  ReadOnly,
  OverwriteOwnedFile,
  CreateDerivativeBeforeMutation,
  SaveAsNewStandaloneFile,
  ProjectControlledGeneration,
  UnsupportedNotYetAvailable
};

enum class GdtfEditorContextKind {
  StandaloneFile,
  ProjectFixture,
  ProjectTruss,
  FutureProjectObject
};

enum class GdtfFieldEditOperation {
  Unsupported,
  ReadOnly,
  DocumentMutation,
  ContextSelection
};

struct GdtfFieldCapability {
  bool visible = false;
  bool editable = false;
  GdtfFieldValueKind valueKind = GdtfFieldValueKind::UnsupportedFuture;
  GdtfFieldEditOperation operation = GdtfFieldEditOperation::Unsupported;
};

struct GdtfEditorContext {
  GdtfEditorContextKind kind = GdtfEditorContextKind::StandaloneFile;
  std::filesystem::path sourcePath;
  GdtfSourceKind sourceKind = GdtfSourceKind::Unknown;
  GdtfWritePolicy writePolicy = GdtfWritePolicy::ReadOnly;
  GdtfDocument document;
  GdtfEditableValues initialValues;
  std::map<GdtfFieldId, GdtfFieldCapability> fieldCapabilities;
  bool editingAllowed = false;
  std::string stableHostId;
  std::string hostLabel;
  std::vector<std::string> diagnostics;
};

struct GdtfApplyRequest {
  GdtfEditorContextKind contextKind = GdtfEditorContextKind::StandaloneFile;
  std::filesystem::path sourcePath;
  GdtfWritePolicy writePolicy = GdtfWritePolicy::ReadOnly;
  GdtfEditableValues values;
  std::set<GdtfFieldId> changedDocumentFields;
  std::set<GdtfFieldId> changedContextFields;
};

struct GdtfApplyResult {
  bool success = false;
  std::vector<std::string> validationErrors;
  std::set<GdtfFieldId> changedGdtfFields;
  std::filesystem::path resultingGdtfPath;
  bool derivativeCreated = false;
  bool projectInstanceResynchronizationRequired = false;
  bool viewerRefreshRequired = false;
  bool hoistLoadRecalculationRequired = false;
  bool projectDirtyStateMustBeUpdated = false;
  std::vector<std::string> diagnostics;
};

const char *ToString(GdtfSourceKind kind);
const char *ToString(GdtfWritePolicy policy);
const GdtfFieldCapability *FindFieldCapability(const GdtfEditorContext &context,
                                               GdtfFieldId fieldId);
GdtfFieldCapability MakeReadOnlyCapability(GdtfFieldValueKind valueKind);
GdtfFieldCapability MakeDocumentEditCapability();
GdtfFieldCapability MakeContextSelectionCapability();

} // namespace gdtf
