#include "project_fixture_gdtf_apply_adapter.h"

#include "filesystem_path_utils.h"

#include <cmath>
#include <string>
#include <system_error>
#include <utility>

namespace gdtf {
namespace {

// Reports whether a changed-field set includes a specific field.
bool ContainsField(const std::set<GdtfFieldId> &fields, GdtfFieldId field) {
  return fields.count(field) > 0;
}

// Checks whether all changed fields are owned by the fixture adapter.
bool HasOnlySupportedFields(const GdtfApplyRequest &request,
                            std::string &unsupportedField) {
  const std::set<GdtfFieldId> documentFields = {
      GdtfFieldId::Weight, GdtfFieldId::PowerConsumption};
  const std::set<GdtfFieldId> contextFields = {
      GdtfFieldId::FixtureTypeName, GdtfFieldId::SourceFileReference,
      GdtfFieldId::ModeName};
  for (const auto field : request.changedDocumentFields) {
    if (!documentFields.count(field)) {
      unsupportedField = std::to_string(static_cast<int>(field));
      return false;
    }
  }
  for (const auto field : request.changedContextFields) {
    if (!contextFields.count(field)) {
      unsupportedField = std::to_string(static_cast<int>(field));
      return false;
    }
  }
  return true;
}

// Reports whether the fixture belongs to the same resulting type/source family.
bool MatchesResultingFamily(const Fixture &fixture,
                            const std::string &originalSource,
                            const std::string &resultingSource,
                            const std::string &resultingType) {
  if (!resultingSource.empty() && fixture.gdtfSpec == resultingSource)
    return true;
  return !originalSource.empty() && fixture.gdtfSpec == originalSource &&
         !resultingType.empty() && fixture.typeName == resultingType;
}

// Adds a validation message to a failed fixture apply result.
ProjectFixtureGdtfApplyResult Fail(std::string message) {
  ProjectFixtureGdtfApplyResult result;
  result.common.success = false;
  result.common.validationErrors.push_back(message);
  result.common.diagnostics.push_back(std::move(message));
  return result;
}

// Returns a human-readable position name for hoist recalculation diagnostics.
std::string EffectivePositionName(const Fixture &fixture) {
  return fixture.positionName.empty() ? "Unassigned" : fixture.positionName;
}

} // namespace

// Stores the injected non-GUI services used by the fixture apply adapter.
ProjectFixtureGdtfApplyAdapter::ProjectFixtureGdtfApplyAdapter(
    ProjectFixtureGdtfApplyServices services)
    : services_(std::move(services)) {}

// Applies a session request to fixture project data after policy preflight.
ProjectFixtureGdtfApplyResult ProjectFixtureGdtfApplyAdapter::Apply(
    const ProjectFixtureGdtfApplyInput &input) const {
  const auto &request = input.request;
  if (request.contextKind != GdtfEditorContextKind::ProjectFixture)
    return Fail("GDTF apply request is not bound to a project fixture.");
  if (request.stableHostId.empty())
    return Fail("GDTF apply request is missing a stable fixture UUID.");
  if (!input.fixtures)
    return Fail("Fixture apply input is missing project fixture data.");
  auto targetIt = input.fixtures->find(request.stableHostId);
  if (targetIt == input.fixtures->end())
    return Fail("Target fixture UUID was not found.");
  std::string unsupportedField;
  if (!HasOnlySupportedFields(request, unsupportedField))
    return Fail("Fixture apply request contains unsupported field: " +
                unsupportedField + ".");

  const bool weightChanged = ContainsField(request.changedDocumentFields, GdtfFieldId::Weight);
  const bool powerChanged = ContainsField(request.changedDocumentFields, GdtfFieldId::PowerConsumption);
  const bool documentChanged = weightChanged || powerChanged;
  const bool typeChanged = ContainsField(request.changedContextFields, GdtfFieldId::FixtureTypeName);
  const bool sourceChanged = ContainsField(request.changedContextFields, GdtfFieldId::SourceFileReference);
  const bool modeChanged = ContainsField(request.changedContextFields, GdtfFieldId::ModeName);

  const Fixture original = targetIt->second;
  const std::string resultingType = request.values.fixtureTypeName.value_or(original.typeName);
  const std::string resultingMode = request.values.modeName.value_or(original.gdtfMode);
  const std::string resultingSource = request.values.sourceFileReference.value_or(original.gdtfSpec);
  const float resultingWeight = request.values.weightKg.value_or(original.weightKg);
  const float resultingPower = request.values.powerConsumptionW.value_or(original.powerConsumptionW);

  if (resultingType.empty())
    return Fail("Fixture type name cannot be empty.");
  if (!std::isfinite(resultingWeight) || resultingWeight < 0.0f)
    return Fail("Weight must be finite and non-negative.");
  if (!std::isfinite(resultingPower) || resultingPower < 0.0f)
    return Fail("Power consumption must be finite and non-negative.");

  std::error_code ec;
  if ((documentChanged || sourceChanged || modeChanged) &&
      (request.sourcePath.empty() || !std::filesystem::is_regular_file(request.sourcePath, ec)))
    return Fail("Resolved GDTF source path is not an existing regular file.");

  auto preparedFixtures = *input.fixtures;
  auto preparedTargetIt = preparedFixtures.find(request.stableHostId);

  ProjectFixtureGdtfApplyResult result;
  result.common.success = true;
  result.common.resultingGdtfPath = request.sourcePath;
  result.resultingType = resultingType;
  result.resultingMode = resultingMode;
  result.resultingSourceReference = resultingSource;
  result.resultingWeightKg = resultingWeight;
  result.resultingPowerConsumptionW = resultingPower;

  if (!resultingMode.empty() && services_.modeExists &&
      !services_.modeExists(request.sourcePath, resultingMode))
    return Fail("Selected GDTF mode does not exist in the resolved document.");
  if (!resultingMode.empty() && services_.channelCount)
    result.derivedChannelCount = services_.channelCount(request.sourcePath, resultingMode);

  std::filesystem::path writablePath = request.sourcePath;
  if (documentChanged) {
    if (request.writePolicy == GdtfWritePolicy::ReadOnly)
      return Fail("Read-only GDTF sources cannot be mutated.");
    if (request.writePolicy == GdtfWritePolicy::CreateDerivativeBeforeMutation) {
      if (!services_.createDerivative)
        return Fail("Derivative creation service is not available.");
      std::string diagnostic;
      if (!services_.createDerivative(request.sourcePath, writablePath, diagnostic))
        return Fail(diagnostic.empty() ? "Could not create a writable GDTF derivative." : diagnostic);
      result.common.derivativeCreated = writablePath != request.sourcePath;
      result.externalFileCreatedOrModified = true;
      result.common.resultingGdtfPath = writablePath;
      result.resultingSourceReference = PathUtils::PathToUtf8(writablePath);
    } else if (request.writePolicy != GdtfWritePolicy::OverwriteOwnedFile) {
      return Fail("The GDTF write policy is not supported for fixture mutation.");
    } else if (request.sourceKind != GdtfSourceKind::PerastageGeneratedDerivative) {
      return Fail("OverwriteOwnedFile requires an explicitly owned Perastage derivative source.");
    }
    if (!services_.writePhysicalProperties)
      return Fail("Physical-property mutation service is not available.");
    std::string diagnostic;
    if (!services_.writePhysicalProperties(writablePath, resultingWeight, resultingPower, diagnostic)) {
      auto failed = Fail(diagnostic.empty() ? "Could not update GDTF physical properties." : diagnostic);
      failed.common.derivativeCreated = result.common.derivativeCreated;
      failed.externalFileCreatedOrModified = result.externalFileCreatedOrModified;
      failed.common.resultingGdtfPath = writablePath;
      return failed;
    }
    result.physicalPropertiesWritten = true;
    result.externalFileCreatedOrModified = true;
    result.common.changedGdtfFields = request.changedDocumentFields;
  }

  preparedTargetIt->second.typeName = resultingType;
  preparedTargetIt->second.gdtfMode = resultingMode;
  preparedTargetIt->second.gdtfSpec = result.resultingSourceReference;
  result.contextSelectionChanged = typeChanged || sourceChanged || modeChanged || result.common.derivativeCreated;

  if (documentChanged) {
    for (auto &[uuid, fixture] : preparedFixtures) {
      if (!MatchesResultingFamily(fixture, original.gdtfSpec,
                                  result.resultingSourceReference,
                                  resultingType) &&
          uuid != request.stableHostId)
        continue;
      if (std::fabs(fixture.weightKg - resultingWeight) > 0.001f) {
        result.weightChangedFixtureUuids.insert(uuid);
        result.changedWeightPositionNames.insert(EffectivePositionName(fixture));
      }
      fixture.typeName = resultingType;
      fixture.gdtfSpec = result.resultingSourceReference;
      fixture.weightKg = resultingWeight;
      fixture.powerConsumptionW = resultingPower;
      fixture.physicalPropertiesSource = FixturePhysicalPropertiesSource::Gdtf;
      fixture.physicalPropertiesDirty = false;
      result.affectedFixtureUuids.insert(uuid);
    }
    result.physicalPropagationOccurred = !result.affectedFixtureUuids.empty();
  } else if (result.contextSelectionChanged) {
    result.affectedFixtureUuids.insert(request.stableHostId);
  }

  const bool changed = documentChanged || result.contextSelectionChanged;
  result.common.projectInstanceResynchronizationRequired = changed;
  result.common.viewerRefreshRequired = changed;
  result.common.hoistLoadRecalculationRequired = !result.changedWeightPositionNames.empty();
  result.common.projectDirtyStateMustBeUpdated = changed;
  *input.fixtures = std::move(preparedFixtures);
  return result;
}

} // namespace gdtf
