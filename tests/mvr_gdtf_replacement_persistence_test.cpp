/* This file is part of Perastage. */
#include "configmanager.h"
#include "file_import_utils.h"
#include "filesystem_path_utils.h"
#include "gdtf_resolution_resources.h"
#include "mvr_import_resource_resolver.h"
#include "mvrexporter.h"
#include "mvrimporter.h"
#include "mvr_exporter_geometry_test_support.h"
#include "support/archive_entry_test_utils.h"
#include "support/gdtf_test_fixture_builder.h"
#include "support/zip_test_utils.h"
#include <tinyxml2.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

namespace {
namespace fs = std::filesystem;
constexpr auto layerId = "10000000-0000-4000-8000-000000000001";
constexpr auto groupId = "20000000-0000-4000-8000-000000000001";
constexpr auto objectId = "30000000-0000-4000-8000-000000000001";
constexpr auto trussId = "40000000-0000-4000-8000-000000000001";

std::string Bytes(const fs::path &path) {
  std::ifstream input(path, std::ios::binary);
  assert(input);
  return {std::istreambuf_iterator<char>(input), {}};
}
void Write(const fs::path &path, const std::string &bytes) {
  std::ofstream output(path, std::ios::binary);
  assert(output.write(bytes.data(), bytes.size()));
}
void Archive(const fs::path &path, const std::vector<std::pair<std::string, std::string>> &entries) {
  std::string error;
  assert(tests::archive::WriteStoredZipWithRawNames(path, entries, error));
}
std::string FixtureId(int type, int instance) {
  return "50000000-0000-4000-8000-0000000000" + std::to_string(type) + std::to_string(instance);
}
// Distinct manufacturers, models, payloads and mode footprints; no network involved.
void Replacement(const fs::path &path, int type) {
  const auto builder = tests::gdtf::BuildMinimalValidFixture().WithModelResource("body").WithFixtureIdentity(
      "Replacement" + std::to_string(type), "Manufacturer" + std::to_string(type),
      "60000000-0000-4000-8000-00000000000" + std::to_string(type));
  tinyxml2::XMLDocument doc;
  assert(doc.Parse(builder.BuildDescriptionXml().c_str()) == tinyxml2::XML_SUCCESS);
  auto *modes = doc.RootElement()->FirstChildElement("FixtureType")->FirstChildElement("DMXModes");
  auto *basic = modes->FirstChildElement("DMXMode");
  basic->SetAttribute("Name", "Basic");
  auto *extended = basic->DeepClone(&doc)->ToElement();
  extended->SetAttribute("Name", "Extended 17");
  extended->FirstChildElement("DMXChannels")->FirstChildElement("DMXChannel")->SetAttribute("Offset", "17");
  modes->InsertEndChild(extended);
  tinyxml2::XMLPrinter xml;
  doc.Print(&xml);
  Archive(path, {{"description.xml", xml.CStr()}, {"models/gltf/body.glb", tests::gdtf::BuildMinimalValidGlb()}});
}
MvrScene Import(const fs::path &path) {
  MvrImporter importer;
  MvrImportResult result;
  MvrImportOptions options;
  options.promptConflicts = false;
  options.applyDictionary = false;
  assert(importer.ImportFromFile(path.string(), result, MvrImportMode::ParseOnly, options));
  return std::move(result.scene);
}
// Checks actual selected resource identity, mode, UUIDs, patch, transform and other content.
void CheckScene(const MvrScene &scene, const std::map<int, std::string> &hashes) {
  assert(scene.fixtures.size() == 18 && scene.trusses.size() == 1 && scene.sceneObjects.size() == 1);
  assert(scene.groupObjects.contains(groupId) && scene.layers.contains(layerId));
  for (const auto &reference : {scene.trusses.at(trussId).symbolFile,
                              scene.sceneObjects.at(objectId).modelFile}) {
    const auto resource = PathUtils::ResolveSceneResourcePath(reference, PathUtils::PathFromUtf8(scene.basePath));
    assert(Bytes(resource) == tests::mvr_geometry::BuildTrussGeometry3ds());
  }
  for (int type = 1; type <= 3; ++type) {
    std::string shared;
    for (int instance = 0; instance < 6; ++instance) {
      const auto &fixture = scene.fixtures.at(FixtureId(type, instance));
      assert(fixture.uuid == FixtureId(type, instance));
      assert(fixture.instanceName == "Instance " + std::to_string(type) + "-" + std::to_string(instance));
      assert(fixture.layerUuid == layerId && fixture.parentGroupUuid == groupId);
      assert(fixture.gdtfMode == (instance % 2 ? "Extended 17" : "Basic"));
      assert(fixture.address == "1." + std::to_string(type * 100 + instance * 17));
      assert((fixture.GetPosition() == std::array<float, 3>{123.0f, 456.0f, 789.0f}));
      const auto source = PathUtils::ResolveSceneResourcePath(fixture.gdtfSpec, PathUtils::PathFromUtf8(scene.basePath));
      assert(FileImportUtils::ComputeFileSha256(source) == hashes.at(type));
      const auto profile = gdtf_resolution::ReadProfile(source);
      assert(profile.model == "Replacement" + std::to_string(type));
      assert(gdtf_resolution::HasMode(profile, fixture.gdtfMode));
      assert(profile.modes.size() == 2 && profile.modes[1].footprint == 17);
      if (shared.empty()) shared = PathUtils::BuildFilesystemIdentityKey(source);
      assert(shared == PathUtils::BuildFilesystemIdentityKey(source));
    }
  }
}
void CheckMvr(const fs::path &path, const std::map<int, std::string> &hashes) {
  std::string pathError;
  const auto names = tests::archive::ReadRawCentralDirectoryEntryNames(
      PathUtils::PathToUtf8(path), pathError);
  assert(pathError.empty() && !names.empty());
  for (const auto &name : names) {
    assert(name.find('\\') == std::string::npos);
    assert(tests::archive::NormalizePresentedArchivePath(name).ok);
  }
  std::map<std::string, int> expected;
  for (const auto &[type, hash] : hashes) expected[hash] = 0;
  for (const auto &entry : tests::zip::ReadEntries(path)) {
    assert(entry.name.find('\\') == std::string::npos);
    if (fs::path(entry.name).extension() != ".gdtf") continue;
    const auto hash = FileImportUtils::ComputeBytesSha256(entry.payload.data(), entry.payload.size());
    assert(expected.contains(hash)); // Neither A nor a dummy is acceptable.
    ++expected.at(hash);
  }
  for (const auto &[hash, count] : expected) assert(count == 1);
  const auto reopened = Import(path);
  CheckScene(reopened, hashes);
}
void CheckProject(const fs::path &path, const std::map<int, std::string> &hashes) {
  std::string embedded;
  assert(tests::zip::ReadEntry(path, "config.json"));
  assert(tests::zip::ReadEntry(path, "scene.mvr", &embedded));
  const auto mvr = path.parent_path() / "embedded.mvr";
  Write(mvr, embedded);
  CheckMvr(mvr, hashes);
}
} // namespace

void VerifyGdtfReplacementPersistence() {
  const auto root = fs::temp_directory_path() / "perastage-replacement-persistence";
  fs::remove_all(root);
  fs::create_directories(root / "downloads");
  std::string fixtures;
  for (int type = 1; type <= 3; ++type)
    for (int instance = 0; instance < 6; ++instance)
      fixtures += "<Fixture uuid='" + FixtureId(type, instance) + "' name='Instance " + std::to_string(type) + "-" + std::to_string(instance) + "'><Matrix>{1,0,0}{0,1,0}{0,0,1}{123,456,789}</Matrix>"
          "<GDTFSpec>original" + std::to_string(type) + ".gdtf</GDTFSpec><GDTFMode>Basic</GDTFMode><Addresses><Address>" +
          std::to_string(type * 100 + instance * 17) + "</Address></Addresses></Fixture>";
  const auto geometry = "<Geometries><Geometry3D fileName='body.3ds'/></Geometries>";
  const auto xml = "<GeneralSceneDescription verMajor='1' verMinor='6'><Scene><Layers><Layer uuid='" +
      std::string(layerId) + "' name='Replacement test'><ChildList><GroupObject uuid='" + groupId +
      "'><ChildList>" + fixtures + "</ChildList></GroupObject><Truss uuid='" + trussId + "'>" + geometry +
      "</Truss><SceneObject uuid='" + objectId + "'>" + geometry +
      "</SceneObject></ChildList></Layer></Layers></Scene></GeneralSceneDescription>";
  const auto imported = root / "input.mvr";
  std::vector<std::pair<std::string, std::string>> entries = {
      {"GeneralSceneDescription.xml", xml}, {"body.3ds", tests::mvr_geometry::BuildTrussGeometry3ds()}};
  for (int type = 1; type <= 3; ++type) {
    const auto name = "original" + std::to_string(type) + ".gdtf";
    const auto source = root / "downloads" / name;
    // A and B share identity metadata but differ in actual package content.
    tests::gdtf::BuildMinimalValidFixture().WithDmxMode("Basic", "Root")
        .WithFixtureIdentity("Replacement" + std::to_string(type), "Manufacturer" + std::to_string(type),
            "60000000-0000-4000-8000-00000000000" + std::to_string(type)).WriteArchive(source);
    entries.emplace_back(name, Bytes(source));
  }
  Archive(imported, entries);
  auto &config = ConfigManager::Get();
  config.Reset();
  config.GetScene() = Import(imported);
  auto &scene = config.GetScene();
  mvr::MvrImportResourceResolver resolver(PathUtils::PathFromUtf8(scene.basePath));
  std::map<int, std::string> hashes;
  for (int type = 1; type <= 3; ++type) {
    const auto download = root / "downloads" / (std::to_string(type) + ".gdtf");
    Replacement(download, type);
    hashes[type] = *FileImportUtils::ComputeFileSha256(download);
    assert(gdtf_resolution::HasMode(gdtf_resolution::ReadProfile(download), "Extended 17"));
    std::string diagnostic;
    // Exercise download/local copy and the application-library external ownership path.
    const auto selected = type == 3 ? download : gdtf_resolution::CopyIntoScene(
        download, PathUtils::PathFromUtf8(scene.basePath), diagnostic);
    assert(!selected.empty());
    for (int instance = 0; instance < 6; ++instance) {
      auto &fixture = scene.fixtures.at(FixtureId(type, instance));
      fixture.gdtfSpec = resolver.MakeSceneRelative(selected);
      fixture.gdtfMode = instance % 2 ? "Extended 17" : "Basic";
    }
  }
  std::string diagnostic;
  assert(resolver.RetainSceneGdtfs(scene, diagnostic));
  fs::remove_all(root / "downloads");
  fs::remove(imported);
  CheckScene(scene, hashes);
  // Simulate old Windows stored references even on a Unix test host.
  for (auto &[uuid, fixture] : scene.fixtures) {
    const auto portable = fixture.gdtfSpec;
    assert(portable.starts_with("resolved-gdtf/") && portable.find('\\') == std::string::npos);
    std::replace(fixture.gdtfSpec.begin(), fixture.gdtfSpec.end(), '/', '\\');
    assert(resolver.ResolveScenePath(portable) == resolver.ResolveScenePath(fixture.gdtfSpec));
  }
  const auto project = root / "first.pstg";
  assert(config.SaveProject(project.string()));
  CheckProject(project, hashes);
  const auto extraction = scene.basePath;
  config.Reset();
  assert(!fs::exists(extraction)); // Restart: no original/import/download workspace survives.
  fs::create_directories(root / "moved");
  const auto moved = root / "moved" / "first.pstg";
  fs::rename(project, moved);
  assert(config.LoadProject(moved.string()));
  CheckScene(config.GetScene(), hashes);
  const auto second = root / "second.pstg";
  assert(config.SaveProject(second.string()));
  CheckProject(second, hashes);
  config.Reset();
  assert(config.LoadProject(second.string()));
  CheckScene(config.GetScene(), hashes);
  const auto exported = root / "final.mvr";
  MvrExporter exporter;
  assert(exporter.ExportToFile(exported.string()));
  CheckMvr(exported, hashes);

  const auto validProject = Bytes(second), validExport = Bytes(exported);
  const auto binding = [&]() -> Fixture & {
    return config.GetScene().fixtures.at(FixtureId(1, 0));
  };
  const auto reference = binding().gdtfSpec;
  const auto mode = binding().gdtfMode;
  const auto selected = PathUtils::ResolveSceneResourcePath(reference, PathUtils::PathFromUtf8(config.GetScene().basePath));
  const auto selectedBytes = Bytes(selected);
  for (const auto &bad : {std::string("missing.gdtf"), std::string("..\\escape.gdtf"),
                         std::string("resolved-gdtf/../alias.gdtf"), std::string("missing-dir/") + reference}) {
    binding().gdtfSpec = bad;
    assert(!config.SaveProject(second.string()));
    assert(!exporter.ExportToFile(exported.string()));
    assert(binding().gdtfSpec == bad && binding().gdtfMode == mode);
    assert(Bytes(second) == validProject && Bytes(exported) == validExport);
    assert(std::any_of(exporter.GetExportDiagnostics().begin(), exporter.GetExportDiagnostics().end(),
        [](const auto &item) { return item.severity == MvrExportDiagnosticSeverity::Error; }));
  }
  binding().gdtfSpec = reference;
  // Check real permission failures where the host filesystem enforces them.
  std::error_code permissionError;
  const auto permissions = fs::status(selected).permissions();
  fs::permissions(selected, fs::perms::none, permissionError);
  if (!permissionError) {
    std::ifstream probe(selected, std::ios::binary);
    if (!probe) {
      assert(!config.SaveProject(second.string()));
      assert(Bytes(second) == validProject);
    }
    fs::permissions(selected, permissions);
  }
  // Missing bytes and unreadable-as-file resources cannot publish a dummy.
  fs::remove(selected);
  assert(!config.SaveProject(second.string()));
  fs::create_directory(selected);
  assert(!config.SaveProject(second.string()));
  fs::remove(selected);
  Write(selected, "invalid archive");
  assert(!config.SaveProject(second.string()));
  assert(Bytes(second) == validProject && binding().gdtfSpec == reference && binding().gdtfMode == mode);
  Archive(selected, {{"../escaped.bin", "unsafe"}, {"description.xml", "<GDTF/>"}});
  assert(!config.SaveProject(second.string()));
  assert(Bytes(second) == validProject);
  Write(selected, selectedBytes);
  assert(config.SaveProject(second.string()));
  CheckProject(second, hashes);
  config.Reset();
  std::ofstream expected(root / "selected.sha256");
  for (const auto &[type, hash] : hashes) expected << type << ' ' << hash << '\n';
  assert(expected.good());
}

// CTest launches this phase in a new process, without any application caches.
void VerifyGdtfReplacementProcessReload() {
  const auto root = fs::temp_directory_path() / "perastage-replacement-persistence";
  std::ifstream expected(root / "selected.sha256");
  std::map<int, std::string> hashes;
  int type;
  std::string hash;
  while (expected >> type >> hash) hashes[type] = hash;
  expected.close();
  assert(hashes.size() == 3);
  auto &config = ConfigManager::Get();
  assert(config.LoadProject((root / "second.pstg").string()));
  CheckScene(config.GetScene(), hashes);
  MvrExporter exporter;
  const auto exported = root / "after-restart.mvr";
  assert(exporter.ExportToFile(exported.string()));
  CheckMvr(exported, hashes);
  config.Reset();
  fs::remove_all(root);
}
