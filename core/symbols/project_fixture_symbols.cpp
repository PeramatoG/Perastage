#include "project_fixture_symbols.h"

#include "file_import_utils.h"
#include "json.hpp"
#include "mvrscene.h"
#include "uuidutils.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <tinyxml2.h>

namespace symbols {
namespace {
using Json = nlohmann::json;
constexpr const char *kIndexPath = "resources/fixture_symbols/index.json";
constexpr std::array<SymbolViewKind, 4> kViews = {
    SymbolViewKind::Top, SymbolViewKind::Front,
    SymbolViewKind::Left, SymbolViewKind::Bottom};
constexpr std::array<const char *, 4> kViewNames = {
    "top", "front", "side", "bottom"};

bool Fail(std::string &error, const std::string &message) {
  error = "Project fixture symbols: " + message;
  return false;
}

bool IsContentId(const std::string &id) {
  return id.size() == 71 && id.starts_with("sha256-") &&
         std::all_of(id.begin() + 7, id.end(), [](char c) {
           return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
         });
}

bool ValidateBundle(const ProjectFixtureSymbolBundle &bundle,
                    std::string &error) {
  if (bundle.schemaVersion != kProjectFixtureSymbolSchemaVersion ||
      bundle.generatorVersion.empty())
    return Fail(error, "unsupported schema or absent generator version");
  std::set<SymbolViewKind> seen;
  size_t availableViews = 0;
  for (const auto &view : bundle.views) {
    if (std::find(kViews.begin(), kViews.end(), view.viewKind) == kViews.end() ||
        !seen.insert(view.viewKind).second ||
        !std::isfinite(view.offsetXmm) || !std::isfinite(view.offsetYmm))
      return Fail(error, "bundle requires unique view identities and finite offsets");
    if (view.svg.empty())
      continue;
    ++availableViews;
    tinyxml2::XMLDocument document;
    if (document.Parse(view.svg.data(), view.svg.size()) != tinyxml2::XML_SUCCESS ||
        !document.RootElement() ||
        std::string(document.RootElement()->Name()) != "svg")
      return Fail(error, "stored SVG is not a valid SVG document");
  }
  return availableViews > 0 || Fail(error, "bundle has no available SVG views");
}

Json BundleManifest(const ProjectFixtureSymbolBundle &bundle,
                    bool includeSvg = false) {
  Json manifest = {
      {"schema_version", bundle.schemaVersion},
      {"generator_version", bundle.generatorVersion},
      {"kind", bundle.kind == ProjectFixtureSymbolKind::UserOverride
                   ? "user_override" : "generated_fallback"},
      {"source_fixture_type_id", bundle.sourceFixtureTypeId},
      {"source_fingerprint", bundle.sourceFingerprint},
      {"views", Json::array()}};
  for (size_t index = 0; index < kViews.size(); ++index) {
    const auto *view = bundle.FindView(kViews[index]);
    if (!view)
      continue;
    Json entry = {{"view", kViewNames[index]},
                  {"file", std::string(kViewNames[index]) + ".svg"},
                  {"offset_x_mm", view->offsetXmm},
                  {"offset_y_mm", view->offsetYmm}};
    if (!view->sourceArchivePath.empty())
      entry["source_archive_path"] = view->sourceArchivePath;
    if (!view->sourceProvenance.empty())
      entry["source_provenance"] = view->sourceProvenance;
    if (includeSvg)
      entry["svg"] = view->svg;
    manifest["views"].push_back(std::move(entry));
  }
  return manifest;
}

std::string ContentId(const ProjectFixtureSymbolBundle &bundle) {
  const auto bytes = BundleManifest(bundle, true).dump();
  return "sha256-" + FileImportUtils::ComputeBytesSha256(bytes.data(), bytes.size());
}

ProjectArchiveResource TextResource(const std::string &path,
                                    const std::string &text) {
  return {path, {text.begin(), text.end()}, true};
}

bool ParseBundle(const std::string &contentId,
                 const std::map<std::string, const ProjectArchiveResource *> &entries,
                 ProjectFixtureSymbolBundle &bundle, std::string &error) {
  const std::string root = std::string(kProjectFixtureSymbolResourceRoot) +
                           contentId + '/';
  const auto manifestEntry = entries.find(root + "manifest.json");
  if (manifestEntry == entries.end())
    return Fail(error, "referenced bundle manifest is missing");
  try {
    const auto &bytes = manifestEntry->second->bytes;
    const Json manifest = Json::parse(bytes.begin(), bytes.end());
    bundle.schemaVersion = manifest.at("schema_version").get<int>();
    bundle.generatorVersion = manifest.at("generator_version").get<std::string>();
    bundle.sourceFixtureTypeId = manifest.at("source_fixture_type_id").get<std::string>();
    bundle.sourceFingerprint = manifest.at("source_fingerprint").get<std::string>();
    const std::string kind = manifest.at("kind").get<std::string>();
    if (kind != "user_override" && kind != "generated_fallback")
      return Fail(error, "unknown bundle ownership kind");
    bundle.kind = kind == "user_override" ? ProjectFixtureSymbolKind::UserOverride
                                           : ProjectFixtureSymbolKind::GeneratedFallback;
    const Json &views = manifest.at("views");
    if (!views.is_array() || views.empty() || views.size() > kViews.size())
      return Fail(error, "bundle manifest must contain available supported views");
    std::set<size_t> parsedViews;
    for (const Json &metadata : views) {
      const std::string name = metadata.at("view").get<std::string>();
      const auto found = std::find(kViewNames.begin(), kViewNames.end(), name);
      if (found == kViewNames.end())
        return Fail(error, "bundle view identity is unsupported");
      const size_t index = static_cast<size_t>(found - kViewNames.begin());
      const std::string expectedFile = std::string(kViewNames[index]) + ".svg";
      if (!parsedViews.insert(index).second || metadata.at("file") != expectedFile)
        return Fail(error, "bundle view resource identity is invalid");
      const auto viewEntry = entries.find(root + expectedFile);
      if (viewEntry == entries.end())
        return Fail(error, "referenced SVG view is missing");
      auto &view = bundle.views[index];
      view.svg.assign(viewEntry->second->bytes.begin(), viewEntry->second->bytes.end());
      view.offsetXmm = metadata.at("offset_x_mm").get<double>();
      view.offsetYmm = metadata.at("offset_y_mm").get<double>();
      view.sourceArchivePath = metadata.value("source_archive_path", "");
      view.sourceProvenance = metadata.value("source_provenance", "");
    }
  } catch (const std::exception &) {
    return Fail(error, "malformed authoritative bundle manifest");
  }
  return ValidateBundle(bundle, error) &&
         (ContentId(bundle) == contentId || Fail(error, "bundle content digest does not match"));
}
} // namespace

const ProjectFixtureSymbolView *ProjectFixtureSymbolBundle::FindView(
    SymbolViewKind view) const {
  if (view == SymbolViewKind::Right)
    view = SymbolViewKind::Left;
  if (view == SymbolViewKind::Back)
    view = SymbolViewKind::Top;
  for (const auto &candidate : views)
    if (candidate.viewKind == view && !candidate.svg.empty())
      return &candidate;
  return nullptr;
}

bool ProjectFixtureSymbolStore::ApplyToFixtures(
    MvrScene &scene, const std::vector<std::string> &fixtureUuids,
    const ProjectFixtureSymbolBundle &bundle, std::string &error) {
  if (!ValidateBundle(bundle, error))
    return false;
  if (fixtureUuids.empty())
    return Fail(error, "no explicit fixture definition bindings supplied");
  std::set<std::string> identities;
  std::vector<Fixture *> fixtures;
  for (const auto &uuid : fixtureUuids) {
    const auto found = scene.fixtures.find(uuid);
    if (found == scene.fixtures.end())
      return Fail(error, "explicit fixture binding does not exist in the scene");
    const std::string canonical = CanonicalizeUuid(found->second.uuid);
    if (canonical.empty())
      return Fail(error, "fixture binding does not have a valid UUID");
    identities.insert(canonical);
    fixtures.push_back(&found->second);
  }
  std::string definitionIdentity;
  for (const auto &identity : identities)
    definitionIdentity += identity + '\n';
  const std::string contentId = ContentId(bundle);
  // A copied fixture may still reference the earlier definition revision.
  // Applying an explicit subset must never retarget those other instances.
  definitionIdentity += contentId;
  const std::string definitionId = "definition-" + FileImportUtils::ComputeBytesSha256(
      definitionIdentity.data(), definitionIdentity.size());
  bundles_[contentId] = bundle;
  definitions_[definitionId] = contentId;
  for (auto *fixture : fixtures)
    fixture->projectSymbolDefinitionId = definitionId;
  PruneUnreferenced(scene);
  return true;
}

const ProjectFixtureSymbolBundle *ProjectFixtureSymbolStore::FindForFixture(
    const Fixture &fixture) const {
  return FindForDefinition(fixture.projectSymbolDefinitionId);
}

const ProjectFixtureSymbolBundle *ProjectFixtureSymbolStore::FindForDefinition(
    const std::string &definitionId) const {
  const auto definition = definitions_.find(definitionId);
  if (definition == definitions_.end())
    return nullptr;
  const auto bundle = bundles_.find(definition->second);
  return bundle == bundles_.end() ? nullptr : &bundle->second;
}

std::string ProjectFixtureSymbolStore::ContentIdForDefinition(
    const std::string &definitionId) const {
  const auto definition = definitions_.find(definitionId);
  return definition == definitions_.end() ? std::string{} : definition->second;
}

bool ProjectFixtureSymbolStore::CollectArchiveResources(
    const MvrScene &scene, std::vector<ProjectArchiveResource> &resources,
    std::string &error) const {
  Json index = {{"schema_version", kProjectFixtureSymbolSchemaVersion},
                {"fixture_bindings", Json::object()}, {"definitions", Json::object()}};
  std::set<std::string> contentIds;
  for (const auto &[key, fixture] : scene.fixtures) {
    (void)key;
    if (fixture.projectSymbolDefinitionId.empty())
      continue;
    const std::string uuid = CanonicalizeUuid(fixture.uuid);
    const auto definition = definitions_.find(fixture.projectSymbolDefinitionId);
    if (uuid.empty() || definition == definitions_.end() ||
        !bundles_.contains(definition->second))
      return Fail(error, "scene references a missing authoritative project definition");
    if (index["fixture_bindings"].contains(uuid))
      return Fail(error, "fixture UUID collision prevents durable symbol binding");
    index["fixture_bindings"][uuid] = definition->first;
    index["definitions"][definition->first] = definition->second;
    contentIds.insert(definition->second);
  }
  if (contentIds.empty())
    return true;
  std::vector<ProjectArchiveResource> collected;
  collected.push_back(TextResource(kIndexPath, index.dump()));
  for (const auto &id : contentIds) {
    const auto &bundle = bundles_.at(id);
    const std::string root = std::string(kProjectFixtureSymbolResourceRoot) + id + '/';
    collected.push_back(TextResource(root + "manifest.json", BundleManifest(bundle).dump()));
    for (size_t view = 0; view < kViews.size(); ++view) {
      if (const auto *available = bundle.FindView(kViews[view]))
        collected.push_back(TextResource(root + kViewNames[view] + ".svg", available->svg));
    }
  }
  resources.insert(resources.end(), std::make_move_iterator(collected.begin()),
                   std::make_move_iterator(collected.end()));
  return true;
}

bool ProjectFixtureSymbolStore::AppendReferencedResourceFingerprints(
    const MvrScene &scene,
    std::vector<project_cache::NamedPayloadFingerprint> &fingerprints,
    std::string &error,
    const std::vector<ProjectArchiveResource> *archiveResources) const {
  std::vector<ProjectArchiveResource> referenced;
  if (!CollectArchiveResources(scene, referenced, error))
    return false;
  std::map<std::string, const std::vector<std::uint8_t> *> packaged;
  if (archiveResources) {
    for (const auto &resource : *archiveResources)
      packaged.emplace(resource.entryName, &resource.bytes);
  }
  std::vector<project_cache::NamedPayloadFingerprint> collected;
  for (const auto &resource : referenced) {
    const auto *bytes = &resource.bytes;
    if (archiveResources) {
      const auto found = packaged.find(resource.entryName);
      if (found == packaged.end())
        return Fail(error, "referenced rendering resource has no packaged bytes");
      bytes = found->second;
    }
    collected.push_back({resource.entryName,
        project_cache::FingerprintBytes(bytes->data(), bytes->size())});
  }
  fingerprints.insert(fingerprints.end(),
                      std::make_move_iterator(collected.begin()),
                      std::make_move_iterator(collected.end()));
  return true;
}

bool ProjectFixtureSymbolStore::LoadArchiveResources(
    const std::vector<ProjectArchiveResource> &resources, std::string &error) {
  std::map<std::string, const ProjectArchiveResource *> entries;
  for (const auto &resource : resources) {
    if (!resource.entryName.starts_with(kProjectFixtureSymbolResourceRoot))
      continue;
    if (!entries.emplace(resource.entryName, &resource).second)
      return Fail(error, "duplicate authoritative archive resource");
  }
  ProjectFixtureSymbolStore restored;
  if (entries.empty()) {
    *this = std::move(restored);
    return true;
  }
  const auto indexEntry = entries.find(kIndexPath);
  if (indexEntry == entries.end())
    return Fail(error, "authoritative fixture-symbol index is missing");
  try {
    const auto &bytes = indexEntry->second->bytes;
    const Json index = Json::parse(bytes.begin(), bytes.end());
    if (index.at("schema_version").get<int>() != kProjectFixtureSymbolSchemaVersion ||
        !index.at("fixture_bindings").is_object() || !index.at("definitions").is_object())
      return Fail(error, "unsupported or malformed fixture-symbol index");
    restored.definitions_ = index.at("definitions").get<decltype(definitions_)>();
    restored.loadedFixtureBindings_ = index.at("fixture_bindings").get<decltype(loadedFixtureBindings_)>();
    for (const auto &[definition, id] : restored.definitions_) {
      if (definition.empty() || !IsContentId(id))
        return Fail(error, "invalid project definition or content identity");
      if (!restored.bundles_.contains(id)) {
        ProjectFixtureSymbolBundle bundle;
        if (!ParseBundle(id, entries, bundle, error))
          return false;
        restored.bundles_[id] = std::move(bundle);
      }
    }
    for (const auto &[uuid, definition] : restored.loadedFixtureBindings_) {
      if (CanonicalizeUuid(uuid) != uuid || !restored.definitions_.contains(definition))
        return Fail(error, "fixture binding references an invalid UUID or definition");
    }
  } catch (const std::exception &) {
    return Fail(error, "malformed authoritative fixture-symbol index");
  }
  *this = std::move(restored);
  return true;
}

bool ProjectFixtureSymbolStore::RestoreFixtureBindings(
    MvrScene &scene, std::string &error) const {
  std::map<std::string, Fixture *> fixtureIdentities;
  for (auto &[key, fixture] : scene.fixtures) {
    (void)key;
    const std::string uuid = CanonicalizeUuid(fixture.uuid);
    if (!fixtureIdentities.emplace(uuid, &fixture).second &&
        loadedFixtureBindings_.contains(uuid))
      return Fail(error, "scene fixture UUID collision prevents symbol restoration");
  }
  for (const auto &[uuid, definition] : loadedFixtureBindings_) {
    (void)definition;
    if (!fixtureIdentities.contains(uuid))
      return Fail(error, "stored project symbol fixture is missing from the scene");
  }
  for (auto &[key, fixture] : scene.fixtures) {
    (void)key;
    fixture.projectSymbolDefinitionId.clear();
  }
  for (const auto &[uuid, definition] : loadedFixtureBindings_)
    fixtureIdentities.at(uuid)->projectSymbolDefinitionId = definition;
  return true;
}

void ProjectFixtureSymbolStore::PruneUnreferenced(const MvrScene &scene) {
  std::set<std::string> definitions;
  std::set<std::string> contents;
  for (const auto &[key, fixture] : scene.fixtures) {
    (void)key;
    definitions.insert(fixture.projectSymbolDefinitionId);
  }
  std::erase_if(definitions_, [&](const auto &entry) {
    if (definitions.contains(entry.first)) {
      contents.insert(entry.second);
      return false;
    }
    return true;
  });
  std::erase_if(bundles_, [&](const auto &entry) { return !contents.contains(entry.first); });
  loadedFixtureBindings_.clear();
}

void ProjectFixtureSymbolStore::Clear() {
  bundles_.clear();
  definitions_.clear();
  loadedFixtureBindings_.clear();
}
} // namespace symbols
