#include "mvr_export_resource_collection.h"
#include "support/gdtf_test_fixture_builder.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace mvr_export_resources;

namespace {

// Records a failed expectation without aborting later characterizations.
void Expect(bool condition, const std::string &message, int &failures) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

// Writes binary test content to a temporary resource file.
void WriteFile(const fs::path &path, const std::string &bytes) {
  fs::create_directories(path.parent_path());
  std::ofstream output(path, std::ios::binary);
  output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

// Appends a little-endian integer to a binary fixture.
void AppendUint32(std::string &bytes, std::uint32_t value) {
  for (int shift = 0; shift < 32; shift += 8)
    bytes.push_back(static_cast<char>((value >> shift) & 0xff));
}

// Appends a little-endian 3DS chunk identifier.
void AppendUint16(std::string &bytes, std::uint16_t value) {
  bytes.push_back(static_cast<char>(value & 0xff));
  bytes.push_back(static_cast<char>((value >> 8) & 0xff));
}

// Wraps a payload in one standard 3DS chunk.
std::string Make3dsChunk(std::uint16_t id, const std::string &payload) {
  std::string chunk;
  AppendUint16(chunk, id);
  AppendUint32(chunk, static_cast<std::uint32_t>(payload.size() + 6));
  chunk += payload;
  return chunk;
}

// Builds a minimal 3DS material texture reference fixture.
std::string Make3dsWithTexture(const std::string &textureName) {
  const std::string texture = Make3dsChunk(0xA300, textureName + '\0');
  const std::string map = Make3dsChunk(0xA200, texture);
  const std::string material = Make3dsChunk(0xAFFF, map);
  return Make3dsChunk(0x4D4D, Make3dsChunk(0x3D3D, material));
}

// Builds a minimal GLB containing one JSON chunk.
std::string MakeGlb(const std::string &jsonInput) {
  std::string json = jsonInput;
  while (json.size() % 4 != 0)
    json.push_back(' ');
  std::string glb;
  AppendUint32(glb, 0x46546C67);
  AppendUint32(glb, 2);
  AppendUint32(glb, static_cast<std::uint32_t>(20 + json.size()));
  AppendUint32(glb, static_cast<std::uint32_t>(json.size()));
  AppendUint32(glb, 0x4E4F534A);
  glb += json;
  return glb;
}

// Finds a planned entry by archive path.
const ResourceEntry *FindEntry(const ResourcePlan &plan,
                               const std::string &archivePath) {
  const auto found =
      std::find_if(plan.entries.begin(), plan.entries.end(),
                   [&](const ResourceEntry &entry) {
                     return entry.archivePath == archivePath;
                   });
  return found == plan.entries.end() ? nullptr : &*found;
}

} // namespace

namespace ProjectUtils {

// Supplies an empty fixture library path for this isolated resource test.
fs::path GetBaseLibraryPath(const std::string &) { return {}; }

// Supplies an empty writable fixture path for dictionary-only link coverage.
std::string GetWritableLibraryPath(const std::string &) { return {}; }

} // namespace ProjectUtils

// Characterizes deterministic collection, dependencies, provenance, and GDTF preparation.
int main() {
  int failures = 0;
  const fs::path root = fs::temp_directory_path() /
                        "perastage-mvr-resource-collection-test";
  std::error_code cleanupError;
  fs::remove_all(root, cleanupError);
  fs::create_directories(root);
  runtime_storage::SetRuntimeRootOverrideForTests(root / "runtime");

  std::vector<MvrExportDiagnostic> diagnostics;
  std::vector<std::string> logs;
  ResourceCollection collection(
      root.string(),
      [&](MvrExportDiagnostic diagnostic) {
        diagnostics.push_back(std::move(diagnostic));
      },
      [&](const std::string &message) { logs.push_back(message); });

  const fs::path preserved = root / "preserved.bin";
  const fs::path collision = root / "collision.bin";
  WriteFile(preserved, "preserved");
  WriteFile(collision, "collision");
  const std::string first = collection.RegisterResource(
      preserved.string(), "bad:name.bin", ResourceKind::Model,
      ResourceProvenance::StandardPreserved);
  const std::string reused = collection.RegisterResource(
      preserved.string(), "ignored.bin", ResourceKind::Model,
      ResourceProvenance::StandardPreserved);
  const std::string collided = collection.RegisterResource(
      collision.string(), "BAD_NAME.BIN", ResourceKind::Model,
      ResourceProvenance::StandardPreserved);
  Expect(first == "bad_name.bin", "filename sanitization changed", failures);
  Expect(reused == first, "source reuse was not deterministic", failures);
  Expect(collided == "BAD_NAME_2.BIN",
         "case-insensitive collision suffix changed", failures);
  const std::string longName(150, 'a');
  const std::string truncated = ResourceCollection::SanitizeArchiveFileName(
      longName + ".gdtf", "fixture.gdtf");
  Expect(truncated.size() == 120 && truncated.ends_with(".gdtf"),
         "filename truncation did not preserve the extension", failures);
  Expect(ResourceCollection::NormalizeArchiveEntryPath("./\\models/test.glb") ==
             "models/test.glb",
         "archive path normalization changed", failures);

  ResourcePlan duplicatePlan;
  duplicatePlan.entries = {
      {preserved, "duplicate.bin", ResourceKind::Model,
       ResourceProvenance::StandardPreserved},
      {collision, "duplicate.bin", ResourceKind::Model,
       ResourceProvenance::StandardPreserved}};
  std::vector<MvrExportDiagnostic> duplicateDiagnostics;
  const ResourcePlan deduplicated = FinalizeResourcePlan(
      std::move(duplicatePlan), {"duplicate.bin"},
      [&](MvrExportDiagnostic diagnostic) {
        duplicateDiagnostics.push_back(std::move(diagnostic));
      },
      {});
  Expect(deduplicated.entries.size() == 1 &&
             deduplicated.entries.front().sourcePath == preserved,
         "duplicate resource handling did not keep the first entry", failures);
  Expect(duplicateDiagnostics.size() == 1 &&
             duplicateDiagnostics.front().code ==
                 MvrExportDiagnosticCode::ResourceDuplicate,
         "duplicate resource diagnostic changed", failures);

  const fs::path gltf = root / "model.gltf";
  WriteFile(root / "local.bin", "local");
  WriteFile(gltf,
            R"({"buffers":[{"uri":"local.bin"},{"uri":"data:abc"},{"uri":"http://example/a.bin"},{"uri":"https://example/b.bin"}]})");
  const std::string gltfArchive =
      collection.RegisterModelResource(gltf.string(), "model.gltf");
  const fs::path glb = root / "model.glb";
  WriteFile(root / "glb.bin", "glb");
  WriteFile(glb, MakeGlb(R"({"asset":{"version":"2.0"},"buffers":[{"uri":"glb.bin"}]})"));
  const std::string glbArchive =
      collection.RegisterModelResource(glb.string(), "model.glb");
  const fs::path model3ds = root / "model.3ds";
  WriteFile(root / "texture.png", "texture");
  WriteFile(model3ds, Make3dsWithTexture("texture.png"));
  const std::string threeDsArchive =
      collection.RegisterModelResource(model3ds.string(), "model.3ds");
  const fs::path missingGltf = root / "missing.gltf";
  WriteFile(missingGltf, R"({"buffers":[{"uri":"missing.bin"}]})");
  collection.RegisterModelResource(missingGltf.string(), "missing.gltf");
  Expect(std::any_of(diagnostics.begin(), diagnostics.end(),
                     [](const MvrExportDiagnostic &diagnostic) {
                       return diagnostic.code ==
                                  MvrExportDiagnosticCode::TextureMissing &&
                              diagnostic.resourceName == "missing.bin";
                     }),
         "missing dependency diagnostic changed", failures);

  const std::string primitiveOne = collection.RegisterPrimitiveModelResource(
      " Primitive:Cube ", "object-one");
  const std::string primitiveTwo = collection.RegisterPrimitiveModelResource(
      "primitive:cube", "object-two");
  Expect(primitiveOne == primitiveTwo,
         "normalized primitive resource was not reused", failures);

  tests::gdtf::BuildMinimalValidFixture().WriteArchive(root / "fixture.gdtf");
  const std::string gdtfArchive = collection.RegisterGdtfResource(
      "fixture-uuid", (root / "fixture.gdtf").string(), "fixture.gdtf");

  ResourcePlan pruned = collection.Finalize(
      {first, gltfArchive, glbArchive, threeDsArchive, primitiveOne,
       gdtfArchive});
  Expect(FindEntry(pruned, "local.bin") != nullptr,
         "referenced glTF dependency was pruned", failures);
  Expect(FindEntry(pruned, "glb.bin") != nullptr,
         "referenced GLB dependency was pruned", failures);
  Expect(FindEntry(pruned, "texture.png") != nullptr,
         "referenced 3DS dependency was pruned", failures);
  Expect(FindEntry(pruned, "missing.gltf") == nullptr,
         "unreferenced model was retained", failures);
  Expect(!logs.empty() &&
             logs.back() ==
                 "MVR export resource pruning summary: referenced_paths=9, "
                 "planned_resources_before=11, planned_resources_after=9, "
                 "pruned=2",
         "pruning summary text or count changed", failures);
  const ResourceEntry *preservedEntry = FindEntry(pruned, first);
  const ResourceEntry *primitiveEntry = FindEntry(pruned, primitiveOne);
  const ResourceEntry *preservedGdtf = FindEntry(pruned, gdtfArchive);
  Expect(preservedEntry && preservedEntry->provenance ==
                               ResourceProvenance::StandardPreserved,
         "preserved resource provenance changed", failures);
  Expect(primitiveEntry && primitiveEntry->provenance ==
                               ResourceProvenance::CompatibilityFallback,
         "primitive compatibility provenance changed", failures);
  Expect(primitiveEntry && fs::exists(primitiveEntry->sourcePath),
         "primitive file lifetime ended before packaging", failures);
  Expect(preservedGdtf &&
             preservedGdtf->provenance ==
                 ResourceProvenance::StandardPreserved &&
             preservedGdtf->sourcePath == root / "fixture.gdtf",
         "preserved GDTF state changed before preparation", failures);

  GdtfRewriteRequest rewrite;
  rewrite.hasWeightKg = true;
  rewrite.weightKg = 12.5f;
  GdtfPreparationResult prepared =
      collection.PrepareGdtfResources({{gdtfArchive, rewrite}});
  Expect(prepared.success, "GDTF preparation failed", failures);
  const ResourceEntry *preparedGdtf = FindEntry(prepared.plan, gdtfArchive);
  Expect(preparedGdtf && fs::exists(preparedGdtf->sourcePath),
         "final GDTF source path is not package-ready", failures);
  Expect(preparedGdtf && preparedGdtf->sourcePath != root / "fixture.gdtf",
         "patched GDTF did not use a prepared source", failures);
  Expect(preparedGdtf && preparedGdtf->provenance ==
                            ResourceProvenance::StandardGenerated,
         "prepared GDTF provenance was not generated standard", failures);
  Expect(!prepared.plan.generatedPaths.empty() &&
             !prepared.plan.workspaceLeases.empty(),
         "prepared plan omitted generated-resource lifetime ownership", failures);
  Expect(preparedGdtf &&
             std::find(prepared.plan.generatedPaths.begin(),
                       prepared.plan.generatedPaths.end(),
                       preparedGdtf->sourcePath) !=
                 prepared.plan.generatedPaths.end(),
         "final GDTF path is absent from generated plan resources", failures);
  Expect(preparedGdtf &&
             std::any_of(prepared.plan.workspaceLeases.begin(),
                         prepared.plan.workspaceLeases.end(),
                         [&](const auto &lease) {
                           return lease &&
                                  lease->Path() ==
                                      preparedGdtf->sourcePath.parent_path();
                         }),
         "final GDTF workspace lease is stale", failures);

  ResourceCollection failingCollection(root.string(), {}, {});
  const fs::path invalidGdtf = root / "invalid.gdtf";
  WriteFile(invalidGdtf, "not a zip");
  const std::string invalidArchive = failingCollection.RegisterGdtfResource(
      "invalid", invalidGdtf.string(), "invalid.gdtf");
  failingCollection.Finalize({invalidArchive});
  const GdtfPreparationResult failed =
      failingCollection.PrepareGdtfResources({});
  Expect(!failed.success && failed.failureOperation == "CanonicalizeGdtf",
         "GDTF preparation failure did not propagate", failures);

  runtime_storage::SetRuntimeRootOverrideForTests({});
  fs::remove_all(root, cleanupError);
  return failures == 0 ? 0 : 1;
}
