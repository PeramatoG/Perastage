#include "gdtf/editor/project_fixture_gdtf_apply_adapter.h"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <unordered_map>

namespace {

// Creates a minimal fixture record for adapter tests.
Fixture MakeFixture(std::string uuid, std::string type, std::string source) {
  Fixture fixture;
  fixture.uuid = std::move(uuid);
  fixture.typeName = std::move(type);
  fixture.gdtfSpec = std::move(source);
  fixture.gdtfMode = "Mode A";
  fixture.weightKg = 10.0f;
  fixture.powerConsumptionW = 100.0f;
  fixture.positionName = fixture.uuid == "a" ? "Front" : "Back";
  return fixture;
}

// Creates adapter services with deterministic test behavior.
gdtf::ProjectFixtureGdtfApplyServices MakeServices(bool failDerivative = false,
                                                   bool failWrite = false,
                                                   bool failPublish = false) {
  gdtf::ProjectFixtureGdtfApplyServices services;
  services.modeExists = [](const std::filesystem::path &, const std::string &mode) {
    return mode == "Mode A" || mode == "Mode B";
  };
  services.channelCount = [](const std::filesystem::path &, const std::string &mode) {
    return mode == "Mode B" ? 12 : 8;
  };
  services.prepareDerivative = [failDerivative](const std::filesystem::path &source,
                                               const std::string &fixtureType,
                                               const std::string &,
                                               fixture_gdtf::PreparedDerivative &prepared,
                                               std::string &diagnostic) {
    assert(!fixtureType.empty());
    if (failDerivative) {
      diagnostic = "Derivative failed.";
      return false;
    }
    prepared.workingPath = source.parent_path() / "derived.gdtf.working.1";
    prepared.publishedPath = source.parent_path() / "derived.gdtf";
    prepared.publishedReference = "derived.gdtf";
    std::ofstream(prepared.workingPath).put('d');
    return true;
  };
  services.publishDerivative = [failPublish](
      const fixture_gdtf::PreparedDerivative &prepared,
      std::string &diagnostic) {
    if (failPublish) {
      diagnostic = "Incomplete derivatives cannot be published canonically.";
      return false;
    }
    std::error_code ec;
    std::filesystem::rename(prepared.workingPath, prepared.publishedPath, ec);
    return !ec;
  };
  services.discardDerivative = [](const fixture_gdtf::PreparedDerivative &prepared) {
    std::error_code ec;
    std::filesystem::remove(prepared.workingPath, ec);
  };
  services.writeDocumentMutation = [failWrite](const std::filesystem::path &,
                                            const gdtf::GdtfApplyRequest &,
                                            std::string &diagnostic) {
    if (failWrite) {
      diagnostic = "Document write failed.";
      return false;
    }
    return true;
  };
  return services;
}

// Creates a temporary GDTF file path for filesystem preflight.
std::filesystem::path MakeTempGdtf() {
  auto path = std::filesystem::temp_directory_path() / "perastage-apply-adapter-ü.gdtf";
  std::ofstream(path).put('x');
  return path;
}

} // namespace

// Exercises no-op, context, mutation, propagation, failure, and stable UUID behavior.
int main() {
  const auto source = MakeTempGdtf();
  std::unordered_map<std::string, Fixture> fixtures;
  fixtures.emplace("b", MakeFixture("b", "Other", "other.gdtf"));
  fixtures.emplace("a", MakeFixture("a", "Type", "source.gdtf"));

  gdtf::GdtfApplyRequest request;
  request.contextKind = gdtf::GdtfEditorContextKind::ProjectFixture;
  request.stableHostId = "a";
  request.sourcePath = source;
  request.sourceKind = gdtf::GdtfSourceKind::PerastageGeneratedDerivative;
  request.writePolicy = gdtf::GdtfWritePolicy::OverwriteOwnedFile;
  request.values.fixtureTypeName = "Type";
  request.values.modeName = "Mode A";
  request.values.sourceFileReference = "source.gdtf";
  request.values.weightKg = 10.0f;
  request.values.powerConsumptionW = 100.0f;

  gdtf::ProjectFixtureGdtfApplyAdapter adapter(MakeServices());
  auto result = adapter.Apply({request, &fixtures});
  assert(result.common.success);
  assert(!result.common.projectDirtyStateMustBeUpdated);

  request.changedContextFields.insert(gdtf::GdtfFieldId::ModeName);
  request.values.modeName = "Mode B";
  result = adapter.Apply({request, &fixtures});
  assert(result.common.success);
  assert(fixtures["a"].gdtfMode == "Mode A");
  assert(result.updatedFixtures.at("a").gdtfMode == "Mode B");
  fixtures["a"] = result.updatedFixtures.at("a");
  assert(fixtures["b"].gdtfMode == "Mode A");
  assert(result.derivedChannelCount == 12);

  request.values.modeName = "Missing";
  result = adapter.Apply({request, &fixtures});
  assert(!result.common.success);
  assert(fixtures["a"].gdtfMode == "Mode B");

  fixtures["b"].typeName = "Type";
  fixtures["b"].gdtfSpec = "source.gdtf";
  request.values.modeName = "Mode B";
  request.changedDocumentFields.insert(gdtf::GdtfFieldId::Weight);
  request.values.weightKg = 12.0f;
  result = adapter.Apply({request, &fixtures});
  assert(result.common.success);
  assert(std::fabs(fixtures["a"].weightKg - 10.0f) < 0.001f);
  assert(std::fabs(result.updatedFixtures.at("a").weightKg - 12.0f) < 0.001f);
  assert(std::fabs(result.updatedFixtures.at("b").weightKg - 12.0f) < 0.001f);
  for (const auto &[uuid, fixture] : result.updatedFixtures)
    fixtures[uuid] = fixture;
  assert(result.changedWeightPositionNames.count("Front") == 1);
  assert(result.changedWeightPositionNames.count("Back") == 1);

  request.values.weightKg = -1.0f;
  result = adapter.Apply({request, &fixtures});
  assert(!result.common.success);

  request.values.weightKg = 14.0f;
  request.writePolicy = gdtf::GdtfWritePolicy::CreateDerivativeBeforeMutation;
  result = adapter.Apply({request, &fixtures});
  assert(result.common.success);
  assert(result.common.derivativeCreated);
  assert(fixtures["a"].gdtfSpec.find("derived.gdtf") == std::string::npos);
  assert(result.updatedFixtures.at("a").gdtfSpec.find("derived.gdtf") != std::string::npos);
  assert(result.updatedFixtures.at("a").gdtfSpec.find(".working") ==
         std::string::npos);

  const std::string referenceBeforeFailedPublication = fixtures["a"].gdtfSpec;
  gdtf::ProjectFixtureGdtfApplyAdapter publishFailing(
      MakeServices(false, false, true));
  result = publishFailing.Apply({request, &fixtures});
  assert(!result.common.success);
  assert(fixtures["a"].gdtfSpec == referenceBeforeFailedPublication);
  assert(result.updatedFixtures.empty());
  assert(!std::filesystem::exists(source.parent_path() /
                                  "derived.gdtf.working.1"));

  request.stableHostId = "missing";
  result = adapter.Apply({request, &fixtures});
  assert(!result.common.success);

  request.stableHostId = "a";
  gdtf::ProjectFixtureGdtfApplyAdapter failing(MakeServices(false, true));
  result = failing.Apply({request, &fixtures});
  assert(!result.common.success);
  assert(!result.externalFileCreatedOrModified);
  assert(fixtures["a"].gdtfSpec.find(".working") == std::string::npos);
  return 0;
}
