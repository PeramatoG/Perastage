/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 */
#pragma once

#include "mvr_export_diagnostic.h"
#include "runtime_storage.h"

#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace mvr_export_resources {

enum class ResourceProvenance {
  StandardPreserved,
  StandardGenerated,
  CompatibilityFallback,
};

enum class ResourceKind { Gdtf, Model, ModelDependency, PrimitiveModel };

struct ResourceEntry {
  std::filesystem::path sourcePath;
  std::string archivePath;
  ResourceKind kind{ResourceKind::Model};
  ResourceProvenance provenance{ResourceProvenance::StandardPreserved};
};

struct ResourcePlan {
  std::vector<ResourceEntry> entries;
  std::unordered_map<std::string, std::unordered_set<std::string>>
      modelDependenciesByArchivePath;
  std::unordered_map<std::string, std::string> gdtfArchiveByObjectUuid;
  std::vector<std::filesystem::path> generatedPaths;
  std::vector<runtime_storage::SceneResourceLeasePtr> workspaceLeases;
};

using DiagnosticSink = std::function<void(MvrExportDiagnostic)>;
using InformationalLogSink = std::function<void(const std::string &)>;

class ResourceCollection {
public:
  ResourceCollection(std::string sceneBasePath, DiagnosticSink diagnosticSink,
                     InformationalLogSink informationalLogSink);
  ~ResourceCollection();

  ResourceCollection(const ResourceCollection &) = delete;
  ResourceCollection &operator=(const ResourceCollection &) = delete;

  std::string RegisterResource(
      const std::string &rawSource, const std::string &preferredArchivePath,
      ResourceKind kind = ResourceKind::Model,
      ResourceProvenance provenance = ResourceProvenance::StandardPreserved,
      bool allowReuseBySource = true);
  std::string RegisterGdtfResource(
      const std::string &objectUuid, const std::string &rawGdtfPath,
      const std::string &preferredName, bool allowReuseBySource = true,
      bool usePreferredDerivativeName = false, bool allowFallback = true);
  std::string RegisterModelResource(const std::string &rawModelSource,
                                    const std::string &fallbackArchiveName);
  std::string RegisterPrimitiveModelResource(const std::string &modelRef,
                                             const std::string &objectUuid);
  void AdoptGeneratedResource(
      const std::filesystem::path &path,
      ResourceProvenance provenance = ResourceProvenance::StandardGenerated);
  void AdoptWorkspace(runtime_storage::TemporaryWorkspace workspace);
  void AssociateGdtfArchive(const std::string &objectUuid,
                            const std::string &archivePath);
  ResourcePlan Finalize(const std::unordered_set<std::string> &referencedPaths);

  const std::unordered_map<std::string, std::string> &
  GdtfArchiveByObjectUuid() const;
  bool HasFatalDependencyError() const;

  static std::string SanitizeArchiveFileName(const std::string &input,
                                             const std::string &fallback);
  static std::string NormalizeArchiveEntryPath(std::string path);
  static std::string ResolveFallbackFixtureGdtfPath();
  std::string ResolveSourcePath(const std::string &rawSource) const;
  std::string BuildSourceIdentity(const std::string &sourcePath) const;

private:
  std::string NormalizeSourcePath(const std::string &rawPath) const;
  std::string ResolveExistingResourceSourcePath(
      const std::string &rawSource) const;
  void RegisterModelDependencies(const std::string &resolvedModelSource,
                                 const std::string &modelArchivePath);

  std::string m_sceneBasePath;
  DiagnosticSink m_diagnosticSink;
  InformationalLogSink m_informationalLogSink;
  ResourcePlan m_plan;
  std::unordered_map<std::string, std::string> m_sourceToArchivePath;
  std::unordered_map<std::string, std::string> m_primitiveSourceByToken;
  std::unordered_set<std::string> m_reservedArchivePaths;
  std::vector<std::filesystem::path> m_generatedPaths;
  std::vector<runtime_storage::SceneResourceLeasePtr> m_workspaceLeases;
  std::filesystem::path m_primitiveWorkspace;
  bool m_dependencyRegistrationFailed{false};
};

} // namespace mvr_export_resources
