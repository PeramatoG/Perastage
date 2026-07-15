#include "fixture_gdtf_apply_services.h"

#include "gdtf_mutation_audit.h"
#include "gdtfdictionary.h"
#include "gdtfloader.h"
#include "filesystem_path_utils.h"

#include <algorithm>

namespace gui {

// Builds the GUI-hosted service bridge used by the non-GUI fixture adapter.
gdtf::ProjectFixtureGdtfApplyServices MakeFixtureGdtfApplyServices() {
  gdtf::ProjectFixtureGdtfApplyServices services;
  services.modeExists = [](const std::filesystem::path &path,
                           const std::string &mode) {
    const auto modes = GetGdtfModes(PathUtils::PathToUtf8(path));
    return std::find(modes.begin(), modes.end(), mode) != modes.end();
  };
  services.channelCount = [](const std::filesystem::path &path,
                             const std::string &mode) {
    return GetGdtfModeChannelCount(PathUtils::PathToUtf8(path), mode);
  };
  services.writeDocumentMutation = [](const std::filesystem::path &path,
                                       const gdtf::GdtfApplyRequest &request,
                                       std::string &) {
    GdtfDocumentMutationRequest mutation;
    mutation.descriptionSet = request.changedDocumentFields.count(
                                  gdtf::GdtfFieldId::FixtureTypeDescription) > 0;
    mutation.description = request.values.fixtureTypeDescription.value_or(std::string());
    mutation.weightSet = request.changedDocumentFields.count(gdtf::GdtfFieldId::Weight) > 0;
    mutation.weightKg = request.values.weightKg.value_or(0.0f);
    mutation.powerSet = request.changedDocumentFields.count(
                            gdtf::GdtfFieldId::PowerConsumption) > 0;
    mutation.powerW = request.values.powerConsumptionW.value_or(0.0f);
    mutation.revisionText = "Updated GDTF fixture type document fields from Perastage";
    return MutateGdtfDocument(PathUtils::PathToUtf8(path), mutation,
                              GdtfMutationAudit::BuildPerastageModifiedBy());
  };
  services.createDerivative = [](const std::filesystem::path &source,
                                 const std::string &fixtureType,
                                 const std::string &,
                                 std::filesystem::path &out,
                                 std::string &diagnostic) {
    if (fixtureType.empty()) {
      diagnostic = "Fixture type context is required to create a derivative.";
      return false;
    }
    auto derivative = GdtfDictionary::CreateOrUpdatePerastageLibraryDerivative(
        fixtureType, PathUtils::PathToUtf8(source));
    if (!derivative || derivative->path.empty()) {
      diagnostic = "Could not create a writable GDTF derivative.";
      return false;
    }
    out = derivative->path;
    return true;
  };
  return services;
}

} // namespace gui
