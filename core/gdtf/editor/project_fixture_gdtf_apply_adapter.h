#pragma once

#include "gdtf_editor_context.h"
#include "fixture.h"
#include "fixture_gdtf_derivative_publication.h"

#include <filesystem>
#include <functional>
#include <unordered_map>
#include <set>
#include <string>
#include <vector>

namespace gdtf {

struct ProjectFixtureGdtfApplyServices {
  std::function<bool(const std::filesystem::path &, const std::string &)> modeExists;
  std::function<int(const std::filesystem::path &, const std::string &)> channelCount;
  std::function<bool(const std::filesystem::path &, const GdtfApplyRequest &, std::string &)> writeDocumentMutation;
  std::function<bool(const std::filesystem::path &, const std::string &,
                     const std::string &, fixture_gdtf::PreparedDerivative &,
                     std::string &)> prepareDerivative;
  std::function<bool(const fixture_gdtf::PreparedDerivative &, std::string &)>
      publishDerivative;
  std::function<void(const fixture_gdtf::PreparedDerivative &)>
      discardDerivative;
};

struct ProjectFixtureGdtfApplyInput {
  GdtfApplyRequest request;
  const std::unordered_map<std::string, Fixture> *fixtures = nullptr;
};

struct ProjectFixtureGdtfApplyResult {
  GdtfApplyResult common;
  std::set<std::string> affectedFixtureUuids;
  std::set<std::string> weightChangedFixtureUuids;
  std::set<std::string> changedWeightPositionNames;
  std::unordered_map<std::string, Fixture> updatedFixtures;
  std::string resultingType;
  std::string resultingMode;
  std::string resultingSourceReference;
  float resultingWeightKg = 0.0f;
  float resultingPowerConsumptionW = 0.0f;
  int derivedChannelCount = -1;
  bool physicalPropertiesWritten = false;
  bool contextSelectionChanged = false;
  bool physicalPropagationOccurred = false;
  bool externalFileCreatedOrModified = false;
};

class ProjectFixtureGdtfApplyAdapter {
public:
  explicit ProjectFixtureGdtfApplyAdapter(ProjectFixtureGdtfApplyServices services);
  ProjectFixtureGdtfApplyResult Apply(const ProjectFixtureGdtfApplyInput &input) const;

private:
  ProjectFixtureGdtfApplyServices services_;
};

} // namespace gdtf
