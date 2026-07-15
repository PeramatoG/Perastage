#include "project_truss_gdtf_apply_adapter.h"

#include "filesystem_path_utils.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <set>
#include <system_error>
#include <utility>

namespace gdtf {
namespace {

// Creates a failed truss adapter result with a diagnostic message.
ProjectTrussGdtfApplyResult Fail(std::string message) {
  ProjectTrussGdtfApplyResult result;
  result.common.success = false;
  result.common.validationErrors.push_back(message);
  result.common.diagnostics.push_back(std::move(message));
  return result;
}

// Trims ASCII whitespace from a resource reference.
std::string TrimResourceReference(std::string value) {
  auto isSpace = [](unsigned char ch) { return std::isspace(ch) != 0; };
  value.erase(value.begin(),
              std::find_if(value.begin(), value.end(), [&](char ch) {
                return !isSpace(static_cast<unsigned char>(ch));
              }));
  value.erase(std::find_if(value.rbegin(), value.rend(), [&](char ch) {
                return !isSpace(static_cast<unsigned char>(ch));
              }).base(),
              value.end());
  return value;
}

// Builds a comparison key for scene resource references.
std::string NormalizeResourceReferenceKey(std::string value) {
  value = TrimResourceReference(std::move(value));
  std::replace(value.begin(), value.end(), '\\', '/');
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return value;
}

// Checks whether a truss still uses the same source-only geometry.
bool MatchesSourceOnlyTrussGeometry(const Truss &truss,
                                    const std::string &sourceSymbolKey) {
  return !sourceSymbolKey.empty() && TrimResourceReference(truss.gdtfSpec).empty() &&
         NormalizeResourceReferenceKey(truss.symbolFile) == sourceSymbolKey;
}

// Copies generated type metadata while preserving instance data.
Truss BuildGeneratedTypeUpdate(const Truss &instance, const Truss &generatedType) {
  Truss updated = instance;
  updated.manufacturer = generatedType.manufacturer;
  updated.model = generatedType.model;
  updated.lengthMm = generatedType.lengthMm;
  updated.widthMm = generatedType.widthMm;
  updated.heightMm = generatedType.heightMm;
  updated.weightKg = generatedType.weightKg;
  updated.crossSection = generatedType.crossSection;
  updated.gdtfSpec = generatedType.gdtfSpec;
  updated.modelFile = generatedType.modelFile;
  updated.perastageAuxGdtfArchivePath = generatedType.perastageAuxGdtfArchivePath;
  updated.gdtfMode = generatedType.gdtfMode.empty() ? instance.gdtfMode
                                                    : generatedType.gdtfMode;
  return updated;
}

// Checks whether a changed-field set contains a field.
bool ContainsField(const std::set<GdtfFieldId> &fields, GdtfFieldId field) {
  return fields.count(field) > 0;
}

// Checks whether all changed fields are owned by the truss adapter.
bool HasOnlySupportedFields(const GdtfApplyRequest &request) {
  const std::set<GdtfFieldId> supported = {
      GdtfFieldId::Manufacturer, GdtfFieldId::ModelName,
      GdtfFieldId::TrussLength, GdtfFieldId::TrussWidth,
      GdtfFieldId::TrussHeight, GdtfFieldId::Weight,
      GdtfFieldId::TrussCrossSection};
  for (const auto field : request.changedDocumentFields) {
    if (!supported.count(field))
      return false;
  }
  return request.changedContextFields.empty();
}

// Resolves a project resource reference without falling back to the process cwd.
std::filesystem::path ResolveProjectPath(const std::filesystem::path &basePath,
                                         const std::string &reference) {
  if (reference.empty())
    return {};
  std::filesystem::path path = PathUtils::PathFromUtf8(reference);
  if (path.is_relative())
    return basePath.empty() ? std::filesystem::path() : basePath / path;
  return path;
}

// Checks whether a path exists as a regular file without throwing.
bool IsRegularFile(const std::filesystem::path &path) {
  if (path.empty())
    return false;
  std::error_code ec;
  return std::filesystem::is_regular_file(path, ec) && !ec;
}

// Checks whether any supported GDTF type field is dirty.
bool HasGenerationFieldChange(const GdtfApplyRequest &request) {
  return ContainsField(request.changedDocumentFields, GdtfFieldId::Manufacturer) ||
         ContainsField(request.changedDocumentFields, GdtfFieldId::ModelName) ||
         ContainsField(request.changedDocumentFields, GdtfFieldId::TrussLength) ||
         ContainsField(request.changedDocumentFields, GdtfFieldId::TrussWidth) ||
         ContainsField(request.changedDocumentFields, GdtfFieldId::TrussHeight) ||
         ContainsField(request.changedDocumentFields, GdtfFieldId::Weight) ||
         ContainsField(request.changedDocumentFields, GdtfFieldId::TrussCrossSection);
}

} // namespace

// Stores injected services used for non-GUI truss GDTF apply operations.
ProjectTrussGdtfApplyAdapter::ProjectTrussGdtfApplyAdapter(
    ProjectTrussGdtfApplyServices services)
    : services_(std::move(services)) {}

// Applies a truss GDTF request to a prepared copy and returns it for host commit.
ProjectTrussGdtfApplyResult ProjectTrussGdtfApplyAdapter::Apply(
    const ProjectTrussGdtfApplyInput &input) const {
  const auto &request = input.request;
  if (request.contextKind != GdtfEditorContextKind::ProjectTruss)
    return Fail("GDTF apply request is not bound to a project truss.");
  if (request.stableHostId.empty())
    return Fail("GDTF apply request is missing a stable truss UUID.");
  if (!input.trusses)
    return Fail("Truss apply input is missing project truss data.");
  const auto it = input.trusses->find(request.stableHostId);
  if (it == input.trusses->end())
    return Fail("Target truss UUID was not found.");
  if (request.writePolicy != GdtfWritePolicy::ProjectControlledGeneration)
    return Fail("Project truss apply requires controlled generation policy.");
  if (!HasOnlySupportedFields(request))
    return Fail("Truss apply request contains unsupported changed fields.");

  const Truss original = it->second;
  const std::string originalSymbolKey =
      TrimResourceReference(original.gdtfSpec).empty()
          ? NormalizeResourceReferenceKey(original.symbolFile)
          : std::string();

  Truss prepared = original;
  prepared.manufacturer = request.values.manufacturer.value_or(prepared.manufacturer);
  prepared.model = request.values.modelName.value_or(prepared.model);
  prepared.lengthMm = request.values.trussLengthMm.value_or(prepared.lengthMm);
  prepared.widthMm = request.values.trussWidthMm.value_or(prepared.widthMm);
  prepared.heightMm = request.values.trussHeightMm.value_or(prepared.heightMm);
  prepared.weightKg = request.values.weightKg.value_or(prepared.weightKg);
  prepared.crossSection = request.values.trussCrossSection.value_or(prepared.crossSection);

  if (!std::isfinite(prepared.lengthMm) || prepared.lengthMm < 0.0f ||
      !std::isfinite(prepared.widthMm) || prepared.widthMm < 0.0f ||
      !std::isfinite(prepared.heightMm) || prepared.heightMm < 0.0f)
    return Fail("Truss dimensions must be finite and non-negative.");
  if (!std::isfinite(prepared.weightKg) || prepared.weightKg < 0.0f)
    return Fail("Truss weight must be finite and non-negative.");

  ProjectTrussGdtfApplyResult result;
  result.common.success = true;
  result.resultingTruss = prepared;
  if (!HasGenerationFieldChange(request))
    return result;

  if (!services_.canonicalFileName || !services_.generateGdtf)
    return Fail("Truss generation services are not available.");
  std::error_code ec;
  std::filesystem::create_directories(input.outputRoot, ec);
  if (input.outputRoot.empty() || ec)
    return Fail("Truss GDTF output root is not writable.");

  Truss exportTruss = prepared;
  exportTruss.symbolFile = PathUtils::PathToUtf8(
      ResolveProjectPath(input.projectResourceBasePath, exportTruss.symbolFile));
  exportTruss.modelFile = PathUtils::PathToUtf8(
      ResolveProjectPath(input.projectResourceBasePath, exportTruss.modelFile));
  if (!exportTruss.symbolFile.empty() &&
      !IsRegularFile(PathUtils::PathFromUtf8(exportTruss.symbolFile)))
    return Fail("Truss symbol resource could not be resolved.");
  if (!exportTruss.modelFile.empty() &&
      !IsRegularFile(PathUtils::PathFromUtf8(exportTruss.modelFile)))
    return Fail("Truss model resource could not be resolved.");

  const std::string canonical = services_.canonicalFileName(
      prepared.manufacturer, prepared.model.empty() ? prepared.name : prepared.model,
      prepared.name);
  if (canonical.empty())
    return Fail("Truss canonical GDTF filename could not be built.");
  const std::filesystem::path outputPath = input.outputRoot / canonical;
  const bool existedBefore = std::filesystem::exists(outputPath, ec) && !ec;
  std::string diagnostic;
  if (!services_.generateGdtf(exportTruss, outputPath, diagnostic)) {
    auto failed = Fail(diagnostic.empty() ? "Failed to create truss GDTF." : diagnostic);
    failed.externalFileCreatedOrModified = !existedBefore && std::filesystem::exists(outputPath, ec) && !ec;
    return failed;
  }

  prepared.gdtfSpec = PathUtils::PathToUtf8(outputPath);
  prepared.modelFile = PathUtils::PathToUtf8(outputPath);
  prepared.perastageAuxGdtfArchivePath = canonical;
  if (prepared.gdtfMode.empty())
    prepared.gdtfMode = "Default";

  result.resultingTruss = prepared;
  result.resultingProjectReference = prepared.gdtfSpec;
  if (input.trusses) {
    for (const auto &[uuid, truss] : *input.trusses) {
      if (uuid == request.stableHostId) {
        result.resultingTrusses.emplace_back(uuid, prepared);
      } else if (MatchesSourceOnlyTrussGeometry(truss, originalSymbolKey)) {
        result.resultingTrusses.emplace_back(
            uuid, BuildGeneratedTypeUpdate(truss, prepared));
      }
    }
  }
  result.canonicalFileName = canonical;
  result.generationOccurred = true;
  result.existingOwnedFileUpdated = existedBefore;
  result.newFileCreated = !existedBefore;
  result.externalFileCreatedOrModified = true;
  for (const auto &[uuid, truss] : result.resultingTrusses)
    result.affectedTrussUuids.push_back(uuid);
  if (result.affectedTrussUuids.empty())
    result.affectedTrussUuids.push_back(request.stableHostId);
  result.tableResynchronizationRequired = true;
  result.previewRefreshRequired = true;
  result.common.changedGdtfFields = request.changedDocumentFields;
  result.common.resultingGdtfPath = outputPath;
  result.common.projectInstanceResynchronizationRequired = true;
  result.common.viewerRefreshRequired = true;
  result.common.projectDirtyStateMustBeUpdated = true;
  return result;
}

} // namespace gdtf
