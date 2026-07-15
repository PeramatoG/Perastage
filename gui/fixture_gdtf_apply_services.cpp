#include "fixture_gdtf_apply_services.h"

#include "gdtf_mutation_audit.h"
#include "gdtfdictionary.h"
#include "gdtfloader.h"
#include "filesystem_path_utils.h"

#include <algorithm>
#include <string>
#include <vector>

namespace {

// Adds a changed fixture document field label when the request contains it.
void AddChangedDocumentFieldLabel(const gdtf::GdtfApplyRequest &request,
                                  gdtf::GdtfFieldId field, const char *label,
                                  std::vector<std::string> &labels) {
  if (request.changedDocumentFields.count(field) > 0)
    labels.emplace_back(label);
}

// Joins changed fixture document field labels for revision text.
std::string JoinDocumentFieldLabels(const std::vector<std::string> &labels) {
  std::string joined;
  for (size_t i = 0; i < labels.size(); ++i) {
    if (i > 0)
      joined += (i + 1 == labels.size()) ? " and " : ", ";
    joined += labels[i];
  }
  return joined;
}

// Builds a concise revision summary for fixture document mutations.
std::string BuildFixtureDocumentRevisionText(
    const gdtf::GdtfApplyRequest &request) {
  std::vector<std::string> labels;
  AddChangedDocumentFieldLabel(request,
                               gdtf::GdtfFieldId::FixtureTypeDescription,
                               "FixtureType Description", labels);
  AddChangedDocumentFieldLabel(request, gdtf::GdtfFieldId::Weight, "Weight",
                               labels);
  AddChangedDocumentFieldLabel(request,
                               gdtf::GdtfFieldId::PowerConsumption,
                               "PowerConsumption", labels);
  if (labels.empty())
    return "Updated GDTF fixture type document fields from Perastage";
  return "Updated GDTF " + JoinDocumentFieldLabels(labels) + " from Perastage";
}

} // namespace

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
    mutation.revisionText = BuildFixtureDocumentRevisionText(request);
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
