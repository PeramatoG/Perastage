#include "project_fixture_symbol_migration.h"

#include "project_fixture_symbols.h"
#include "fixture_symbol_resource_contract.h"
#include "fixture_symbol_resolution.h"
#include "file_import_utils.h"
#include "filesystem_path_utils.h"
#include "gdtf_archive_reader.h"
#include "mvrscene.h"

#include <filesystem>
#include <map>
#include <tinyxml2.h>

namespace symbols {
namespace {
std::filesystem::path SourcePath(const MvrScene &scene,
    const Fixture &fixture, const std::string &libraryRoot) {
  const auto spec = PathUtils::PathFromUtf8(fixture.gdtfSpec);
  if (spec.empty())
    return {};
  std::error_code error;
  const auto projectPath = spec.is_absolute() ? spec
      : PathUtils::PathFromUtf8(scene.basePath) / spec;
  if ((spec.is_absolute() || !scene.basePath.empty()) &&
      std::filesystem::is_regular_file(projectPath, error) && !error)
    return projectPath;
  const auto libraryPath = PathUtils::PathFromUtf8(libraryRoot) / spec.filename();
  error.clear();
  return std::filesystem::is_regular_file(libraryPath, error) && !error
      ? libraryPath : std::filesystem::path{};
}

bool ReadLegacyBundle(const std::filesystem::path &source,
    ProjectFixtureSymbolBundle &bundle, bool &found, std::string &error) {
  FixtureSymbolResourceInspection inspection;
  found = false;
  if (source.empty() ||
      !InspectFixtureSymbolResources(PathUtils::PathToUtf8(source), inspection))
    return true;
  bundle.generatorVersion = "legacy-perastage-import-1";
  for (auto &view : bundle.views) {
    const auto *resource = inspection.FindPerastageView(view.viewKind);
    if (!resource || !resource->usable || !resource->PerastageOwned())
      continue;
    const auto bytes = gdtf::ReadGdtfArchiveResource(source, resource->archivePath);
    if (!bytes.Success() || bytes.filesystemFallback ||
        bytes.entryPath != resource->archivePath) {
      error = "A recognized legacy fixture symbol could not be materialized.";
      return false;
    }
    view.svg.assign(bytes.bytes.begin(), bytes.bytes.end());
    view.offsetXmm = resource->offsetXmm;
    view.offsetYmm = resource->offsetYmm;
    view.sourceArchivePath = resource->archivePath;
    view.sourceProvenance = "legacy-perastage";
    found = true;
  }
  if (!found)
    return true;
  // Freeze complementary authored views as rendering snapshots. They remain
  // identified as authored input; recognizing one legacy view does not claim
  // ownership of the original standard resources.
  for (auto &view : bundle.views) {
    if (!view.svg.empty())
      continue;
    const auto resolved = ResolveFixtureSymbolView(inspection, view.viewKind,
        FixtureSymbolResolutionPurpose::InternalRendering);
    if (!resolved.usable || resolved.resolvedView != view.viewKind)
      continue;
    const auto bytes = gdtf::ReadGdtfArchiveResource(source, resolved.archivePath);
    if (!bytes.Success() || bytes.filesystemFallback || bytes.entryPath != resolved.archivePath) {
      error = "A complementary fixture symbol view could not be materialized.";
      return false;
    }
    view.svg.assign(bytes.bytes.begin(), bytes.bytes.end());
    view.offsetXmm = resolved.offsetXmm;
    view.offsetYmm = resolved.offsetYmm;
    view.sourceArchivePath = resolved.archivePath;
    view.sourceProvenance = "authored-gdtf";
  }
  if (const auto fingerprint = FileImportUtils::ComputeFileSha256(source))
    bundle.sourceFingerprint = "sha256:" + *fingerprint;
  const auto archive = gdtf::ReadGdtfArchive(source);
  tinyxml2::XMLDocument document;
  if (document.Parse(archive.descriptionXml.data(), archive.descriptionXml.size()) ==
      tinyxml2::XML_SUCCESS) {
    const auto *root = document.FirstChildElement("GDTF");
    const auto *fixture = root ? root->FirstChildElement("FixtureType") : nullptr;
    if (fixture && fixture->Attribute("FixtureTypeID"))
      bundle.sourceFixtureTypeId = fixture->Attribute("FixtureTypeID");
  }
  return true;
}
} // namespace

bool MaterializeLegacyFixtureSymbols(MvrScene &scene,
    ProjectFixtureSymbolStore &store, const std::string &fixtureLibraryRoot,
    std::string &error) {
  MvrScene stagedScene = scene;
  ProjectFixtureSymbolStore stagedStore = store;
  std::map<std::pair<std::string, std::string>, std::vector<std::string>> definitions;
  for (const auto &[uuid, fixture] : scene.fixtures) {
    if (!store.FindForFixture(fixture))
      definitions[{fixture.gdtfSpec, fixture.gdtfMode}].push_back(uuid);
  }
  bool changed = false;
  for (const auto &[definition, uuids] : definitions) {
    (void)definition;
    ProjectFixtureSymbolBundle bundle;
    bool found = false;
    if (!ReadLegacyBundle(SourcePath(scene, scene.fixtures.at(uuids.front()),
                                    fixtureLibraryRoot), bundle, found, error))
      return false;
    if (found) {
      if (!stagedStore.ApplyToFixtures(stagedScene, uuids, bundle, error))
        return false;
      changed = true;
    }
  }
  if (changed) {
    scene = std::move(stagedScene);
    store = std::move(stagedStore);
  }
  error.clear();
  return true;
}
} // namespace symbols
