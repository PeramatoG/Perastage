#pragma once

#include "project_archive_resource.h"
#include "project_cache_validation.h"
#include "symbol_view_kind.h"

#include <array>
#include <map>
#include <string>
#include <vector>

class MvrScene;
struct Fixture;

namespace symbols {

inline constexpr int kProjectFixtureSymbolSchemaVersion = 1;
inline constexpr char kProjectFixtureSymbolResourceRoot[] =
    "resources/fixture_symbols/";

enum class ProjectFixtureSymbolKind { GeneratedFallback, UserOverride };

struct ProjectFixtureSymbolView {
  SymbolViewKind viewKind = SymbolViewKind::Top;
  std::string svg;
  double offsetXmm = 0.0;
  double offsetYmm = 0.0;
  // Per-view provenance retained when legacy rendering includes authored views.
  std::string sourceArchivePath;
  std::string sourceProvenance;
  bool operator==(const ProjectFixtureSymbolView &) const = default;
};

struct ProjectFixtureSymbolBundle {
  int schemaVersion = kProjectFixtureSymbolSchemaVersion;
  std::string generatorVersion;
  ProjectFixtureSymbolKind kind = ProjectFixtureSymbolKind::GeneratedFallback;
  std::string sourceFixtureTypeId;
  std::string sourceFingerprint;
  std::array<ProjectFixtureSymbolView, 4> views = {{
      {.viewKind = SymbolViewKind::Top},
      {.viewKind = SymbolViewKind::Front},
      {.viewKind = SymbolViewKind::Left},
      {.viewKind = SymbolViewKind::Bottom},
  }};
  const ProjectFixtureSymbolView *FindView(SymbolViewKind view) const;
  bool operator==(const ProjectFixtureSymbolBundle &) const = default;
};

// Authoritative project data. No external file or cache is needed after loading.
class ProjectFixtureSymbolStore {
public:
  bool ApplyToFixtures(MvrScene &scene,
                       const std::vector<std::string> &fixtureUuids,
                       const ProjectFixtureSymbolBundle &bundle,
                       std::string &errorMessage);
  const ProjectFixtureSymbolBundle *FindForFixture(const Fixture &fixture) const;
  const ProjectFixtureSymbolBundle *FindForDefinition(
      const std::string &definitionId) const;
  std::string ContentIdForDefinition(const std::string &definitionId) const;
  bool CollectArchiveResources(const MvrScene &scene,
                               std::vector<ProjectArchiveResource> &resources,
                               std::string &errorMessage) const;
  // Covers authoritative symbol bytes in the existing layout-cache proof.
  // On restore, hashes packaged bytes only for the referenced resource names.
  bool AppendReferencedResourceFingerprints(
      const MvrScene &scene,
      std::vector<project_cache::NamedPayloadFingerprint> &fingerprints,
      std::string &errorMessage,
      const std::vector<ProjectArchiveResource> *archiveResources = nullptr) const;
  // Validates all authoritative bytes before any scene/config callbacks run.
  bool LoadArchiveResources(const std::vector<ProjectArchiveResource> &resources,
                            std::string &errorMessage);
  bool RestoreFixtureBindings(MvrScene &scene, std::string &errorMessage) const;
  void PruneUnreferenced(const MvrScene &scene);
  void Clear();
  size_t BundleCount() const { return bundles_.size(); }

private:
  std::map<std::string, ProjectFixtureSymbolBundle> bundles_;
  std::map<std::string, std::string> definitions_;
  std::map<std::string, std::string> loadedFixtureBindings_;
};

} // namespace symbols
