#include "gdtf/editor/project_fixture_gdtf_apply_adapter.h"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>

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
                                                   bool failWrite = false) {
  gdtf::ProjectFixtureGdtfApplyServices services;
  services.modeExists = [](const std::filesystem::path &, const std::string &mode) {
    return mode == "Mode A" || mode == "Mode B";
  };
  services.channelCount = [](const std::filesystem::path &, const std::string &mode) {
    return mode == "Mode B" ? 12 : 8;
  };
  services.createDerivative = [failDerivative](const std::filesystem::path &source,
                                               std::filesystem::path &out,
                                               std::string &diagnostic) {
    if (failDerivative) {
      diagnostic = "Derivative failed.";
      return false;
    }
    out = source.parent_path() / "derived.gdtf";
    std::ofstream(out).put('d');
    return true;
  };
  services.writePhysicalProperties = [failWrite](const std::filesystem::path &, float,
                                                 float, std::string &diagnostic) {
    if (failWrite) {
      diagnostic = "Physical write failed.";
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
  std::map<std::string, Fixture> fixtures;
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
  assert(fixtures["a"].gdtfMode == "Mode B");
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
  assert(std::fabs(fixtures["a"].weightKg - 12.0f) < 0.001f);
  assert(std::fabs(fixtures["b"].weightKg - 12.0f) < 0.001f);
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
  assert(fixtures["a"].gdtfSpec.find("derived.gdtf") != std::string::npos);

  request.stableHostId = "missing";
  result = adapter.Apply({request, &fixtures});
  assert(!result.common.success);

  request.stableHostId = "a";
  gdtf::ProjectFixtureGdtfApplyAdapter failing(MakeServices(false, true));
  result = failing.Apply({request, &fixtures});
  assert(!result.common.success);
  assert(result.externalFileCreatedOrModified);
  return 0;
}
