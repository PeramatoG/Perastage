#include "gdtf/editor/project_truss_gdtf_apply_adapter.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>

namespace {

// Creates a minimal truss record for adapter tests.
Truss MakeTruss(std::string uuid) {
  Truss truss;
  truss.uuid = std::move(uuid);
  truss.name = "Type A";
  truss.manufacturer = "Maker";
  truss.model = "Model";
  truss.lengthMm = 1000.0f;
  truss.widthMm = 200.0f;
  truss.heightMm = 300.0f;
  truss.weightKg = 12.0f;
  truss.crossSection = "Box";
  truss.gdtfMode = "Default";
  return truss;
}

// Creates deterministic fake services for truss adapter tests.
gdtf::ProjectTrussGdtfApplyServices MakeServices(bool failGeneration = false,
                                                 int *calls = nullptr, std::string *revisionText = nullptr) {
  gdtf::ProjectTrussGdtfApplyServices services;
  services.canonicalFileName = [](const std::string &, const std::string &model,
                                  const std::string &) {
    return model + "@Perastage.gdtf";
  };
  services.generateGdtf = [failGeneration, calls, revisionText](
      const Truss &, const std::filesystem::path &out,
      const std::string &text, std::string &diagnostic) {
    if (revisionText)
      *revisionText = text;
    if (calls)
      ++*calls;
    if (failGeneration) {
      diagnostic = "generation failed";
      return false;
    }
    std::ofstream(out).put('g');
    return true;
  };
  return services;
}

} // namespace

// Exercises validation, no-op, generation, failure, paths, and stable UUID behavior.
int main() {
  const auto root = std::filesystem::temp_directory_path() / "perastage-truss-adapter-ü";
  std::filesystem::create_directories(root / "models");
  std::ofstream(root / "models" / "shared.3ds").put('s');
  std::ofstream(root / "models" / "other.3ds").put('o');
  std::unordered_map<std::string, Truss> trusses;
  Truss other = MakeTruss("other");
  other.symbolFile = "models/other.3ds";
  trusses.emplace("other", other);
  Truss target = MakeTruss("target");
  target.symbolFile = "models/shared.3ds";
  trusses.emplace("target", target);
  Truss shared = MakeTruss("shared");
  shared.symbolFile = "models\\shared.3ds";
  shared.transform.o[0] = 42.0f;
  trusses.emplace("shared", shared);
  Truss alreadyGdtf = MakeTruss("already-gdtf");
  alreadyGdtf.symbolFile = "models/shared.3ds";
  alreadyGdtf.gdtfSpec = "existing.gdtf";
  trusses.emplace("already-gdtf", alreadyGdtf);

  gdtf::GdtfApplyRequest request;
  request.contextKind = gdtf::GdtfEditorContextKind::ProjectTruss;
  request.stableHostId = "target";
  request.writePolicy = gdtf::GdtfWritePolicy::ProjectControlledGeneration;
  request.values.modelName = "Model";
  int calls = 0;
  std::string revisionText;
  gdtf::ProjectTrussGdtfApplyAdapter adapter(MakeServices(false, &calls, &revisionText));
  auto result = adapter.Apply({request, &trusses, root, root});
  assert(result.common.success);
  assert(!result.generationOccurred);
  assert(calls == 0);

  request.changedDocumentFields.insert(gdtf::GdtfFieldId::ModelName);
  request.values.modelName = "ModelB";
  result = adapter.Apply({request, &trusses, root, root});
  assert(result.common.success);
  assert(result.generationOccurred);
  assert(calls == 1);
  assert(result.resultingTruss);
  assert(result.resultingTruss->model == "ModelB");
  assert(revisionText.find("Model") != std::string::npos);
  assert(result.resultingTruss->gdtfSpec.find("ModelB@Perastage.gdtf") != std::string::npos);
  assert(result.resultingTrusses.size() == 2);
  bool sawTarget = false;
  bool sawShared = false;
  for (const auto &[uuid, truss] : result.resultingTrusses) {
    if (uuid == "target")
      sawTarget = truss.model == "ModelB" && !truss.gdtfSpec.empty();
    if (uuid == "shared")
      sawShared = truss.model == "ModelB" && !truss.gdtfSpec.empty() &&
                  truss.transform.o[0] == 42.0f;
    assert(uuid != "already-gdtf");
    assert(uuid != "other");
  }
  assert(sawTarget);
  assert(sawShared);
  assert(trusses["target"].model == "Model");

  request.changedDocumentFields.insert(gdtf::GdtfFieldId::FixtureTypeDescription);
  request.values.fixtureTypeDescription = "New description";
  result = adapter.Apply({request, &trusses, root, root});
  assert(result.common.success);
  assert(revisionText.find("FixtureType Description") != std::string::npos);

  request.values.weightKg = -1.0f;
  request.changedDocumentFields.insert(gdtf::GdtfFieldId::Weight);
  result = adapter.Apply({request, &trusses, root, root});
  assert(!result.common.success);

  request.values.weightKg = 13.0f;
  gdtf::ProjectTrussGdtfApplyAdapter failing(MakeServices(true, &calls));
  result = failing.Apply({request, &trusses, root, root});
  assert(!result.common.success);
  assert(trusses["target"].weightKg == 12.0f);

  request.contextKind = gdtf::GdtfEditorContextKind::ProjectFixture;
  result = adapter.Apply({request, &trusses, root, root});
  assert(!result.common.success);
  return 0;
}
