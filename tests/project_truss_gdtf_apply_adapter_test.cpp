#include "gdtf/editor/project_truss_gdtf_apply_adapter.h"
#include "filesystem_path_utils.h"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <unordered_map>
#include <vector>

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
  services.readWireNames = [](const std::filesystem::path &) {
    return std::optional<fixture_network::WireNames>{fixture_network::WireNames{{"ethernet_1", 1u}}};
  };
  return services;
}

} // namespace

// Exercises validation, no-op, generation, failure, paths, and stable UUID behavior.
int main() {
  const std::string directoryName = "perastage-truss-adapter-\xC3\xBC-\xE7\x81\xAF-" +
      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
  const auto root = std::filesystem::temp_directory_path() / PathUtils::PathFromUtf8(directoryName);
  assert(std::filesystem::create_directory(root));
  assert(PathUtils::PathToUtf8(root.filename()) == directoryName);
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

  for (int guarded = 0; guarded < 5; ++guarded) {
    auto candidates = trusses;
    auto &candidate = candidates.at(guarded == 2 ? "shared" : "target");
    candidate.dmxAddresses.emplace();
    candidate.dmxAddresses->entries = {{std::nullopt, "17"}};
    if (guarded == 0) candidate.gdtfSpec = "authored.gdtf";
    else candidate.dmxAddresses->networks = {{"ethernet_1"}};
    if (guarded >= 3) { candidate.dmxAddresses.reset(); candidate.gdtfSpec = "authored.gdtf"; }
    if (guarded == 4) candidate.perastageAuxGdtfArchivePath = "authored.gdtf";
    const auto originalAssignments = candidate.dmxAddresses;
    const auto originalReference = candidate.gdtfSpec;
    const int previousCalls = calls;
    const auto output = root / ("guarded-" + std::to_string(guarded));
    result = adapter.Apply({request, &candidates, root, output});
    assert(!result.common.success && !result.common.validationErrors.empty());
    assert(!result.generationOccurred && !result.externalFileCreatedOrModified);
    assert(calls == previousCalls && !std::filesystem::exists(output));
    assert(candidate.dmxAddresses == originalAssignments && candidate.gdtfSpec == originalReference);
  }

  auto native = trusses;
  native.at("target").gdtfSpec = "native.gdtf";
  native.at("target").perastageAuxGdtfArchivePath = "native.gdtf";
  auto nativeServices = MakeServices(false, &calls);
  nativeServices.readWireNames = [](const std::filesystem::path &) {
    return std::optional<fixture_network::WireNames>{fixture_network::WireNames{}};
  };
  assert(gdtf::ProjectTrussGdtfApplyAdapter(nativeServices).Apply({request, &native, root, root}).common.success);

  const auto destination = root / "ModelB@Perastage.gdtf";
  const std::string retainedBytes = "retained authored GDTF";
  { std::ofstream(destination, std::ios::binary) << retainedBytes; }
  const auto destinationIdentity = PathUtils::BuildFilesystemIdentityKey(destination);
  std::vector<std::string> references = {
      PathUtils::PathToUtf8(destination), "ModelB@Perastage.gdtf",
      "./models/../ModelB@Perastage.gdtf"};
#ifdef _WIN32
  references.push_back(".\\models\\..\\ModelB@Perastage.gdtf");
  references.push_back("modelb@perastage.GDTF");
#endif
  for (const auto &reference : references) {
    const auto path = PathUtils::PathFromUtf8(reference);
    assert(PathUtils::BuildFilesystemIdentityKey(path, root) == destinationIdentity);
    auto protectedTrusses = native;
    Truss protectedPeer = MakeTruss("protected");
    protectedPeer.gdtfSpec = reference;
    protectedPeer.dmxAddresses.emplace();
    protectedPeer.dmxAddresses->entries = {{std::nullopt, "17"}};
    protectedPeer.dmxAddresses->networks = {{"ethernet_1"}};
    protectedTrusses[protectedPeer.uuid] = protectedPeer;
    const auto previousCalls = calls;
    const auto protectedResult = gdtf::ProjectTrussGdtfApplyAdapter(nativeServices).Apply(
        {request, &protectedTrusses, root, root});
    assert(!protectedResult.common.success && !protectedResult.externalFileCreatedOrModified);
    assert(!protectedResult.generationOccurred && calls == previousCalls);
    assert(protectedResult.common.validationErrors.front().find("retained by Truss protected") != std::string::npos);
    const auto &unchanged = protectedTrusses.at("protected");
    assert(unchanged.dmxAddresses == protectedPeer.dmxAddresses);
    assert(unchanged.gdtfSpec == reference && unchanged.gdtfMode == protectedPeer.gdtfMode);
    assert(unchanged.perastageAuxGdtfArchivePath == protectedPeer.perastageAuxGdtfArchivePath);
    assert(protectedTrusses.at("target").gdtfSpec == native.at("target").gdtfSpec);
    assert(protectedTrusses.at("target").model == native.at("target").model);
    std::ifstream retained(destination, std::ios::binary);
    assert(std::string(std::istreambuf_iterator<char>(retained), {}) == retainedBytes);
  }

  // A UTF-8 canonical filename must resolve to the same native path as its stored reference.
  auto unicodeRequest = request;
  unicodeRequest.values.modelName = "Model-\xE7\x81\xAF";
  const auto previousCalls = calls;
  const auto unicodeResult = gdtf::ProjectTrussGdtfApplyAdapter(nativeServices).Apply(
      {unicodeRequest, &native, root, root});
  assert(unicodeResult.common.success && unicodeResult.generationOccurred);
  assert(calls == previousCalls + 1);
  const auto unicodeDestination = root / PathUtils::PathFromUtf8("Model-\xE7\x81\xAF@Perastage.gdtf");
  assert(std::filesystem::is_regular_file(unicodeDestination));
  assert(PathUtils::BuildFilesystemIdentityKey(unicodeResult.resultingTruss->gdtfSpec) ==
         PathUtils::BuildFilesystemIdentityKey(unicodeDestination));

  request.contextKind = gdtf::GdtfEditorContextKind::ProjectFixture;
  result = adapter.Apply({request, &trusses, root, root});
  assert(!result.common.success);
  std::filesystem::remove_all(root);
  return 0;
}
