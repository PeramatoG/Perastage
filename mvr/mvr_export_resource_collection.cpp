/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 */
#include "mvr_export_resource_collection.h"

#include "filesystem_path_utils.h"
#include "gdtfdictionary.h"
#include "primitive_model_resources.h"
#include "projectutils.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <regex>
#include <sstream>

namespace fs = std::filesystem;

namespace {

struct ThreeDsChunkHeader { uint16_t id{0}; uint32_t length{0}; };

// Trims ASCII whitespace from both ends of a string.
std::string TrimAscii(std::string value) {
  const auto isSpace = [](unsigned char ch) { return std::isspace(ch) != 0; };
  value.erase(value.begin(), std::find_if(value.begin(), value.end(),
                                          [&](char ch) { return !isSpace(ch); }));
  value.erase(std::find_if(value.rbegin(), value.rend(),
                           [&](char ch) { return !isSpace(ch); }).base(), value.end());
  return value;
}

// Converts ASCII characters to lowercase for stable comparisons.
std::string ToLowerAscii(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return value;
}

// Truncates a filename without discarding its extension.
std::string TruncateFileNamePreservingExtension(const std::string &fileName,
                                                std::size_t maxLength) {
  if (fileName.size() <= maxLength)
    return fileName;
  const fs::path path(fileName);
  const std::string extension = path.extension().string();
  if (extension.size() >= maxLength)
    return fileName.substr(0, maxLength);
  std::string stem = path.stem().string();
  stem.resize(maxLength - extension.size());
  return stem + extension;
}

// Tests whether a filename is reserved regardless of ASCII case.
bool ContainsArchiveFileNameCaseInsensitive(
    const std::unordered_set<std::string> &reserved, const std::string &candidate) {
  const std::string lowered = ToLowerAscii(candidate);
  return std::any_of(reserved.begin(), reserved.end(), [&](const std::string &value) {
    return ToLowerAscii(value) == lowered;
  });
}

// Creates a unique root-level archive filename.
std::string EnsureUniqueArchivePath(const std::string &proposed,
                                    std::unordered_set<std::string> &reserved) {
  std::string normalized =
      mvr_export_resources::ResourceCollection::SanitizeArchiveFileName(
          proposed, "resource.bin");
  if (normalized.empty())
    normalized = "resource.bin";
  if (!ContainsArchiveFileNameCaseInsensitive(reserved, normalized)) {
    reserved.insert(normalized);
    return normalized;
  }
  fs::path path(normalized);
  std::string stem = path.stem().string();
  if (stem.empty())
    stem = "resource";
  const std::string extension = path.extension().string();
  for (int suffix = 2;; ++suffix) {
    const std::string suffixText = "_" + std::to_string(suffix);
    std::string adjustedStem = stem;
    constexpr std::size_t kMaximumFileNameLength = 120;
    if (adjustedStem.size() + suffixText.size() + extension.size() >
        kMaximumFileNameLength) {
      adjustedStem.resize(kMaximumFileNameLength - suffixText.size() -
                          extension.size());
    }
    if (adjustedStem.empty())
      adjustedStem = "resource";
    const std::string candidate = adjustedStem + suffixText + extension;
    if (!ContainsArchiveFileNameCaseInsensitive(reserved, candidate)) {
      reserved.insert(candidate);
      return candidate;
    }
  }
}

// Reads a 3DS chunk header from the current stream position.
bool Read3dsChunkHeader(std::ifstream &file, ThreeDsChunkHeader &chunk) {
  return static_cast<bool>(file.read(reinterpret_cast<char *>(&chunk.id), sizeof(chunk.id))) &&
         static_cast<bool>(file.read(reinterpret_cast<char *>(&chunk.length), sizeof(chunk.length)));
}

// Reads a bounded null-terminated string from a 3DS chunk.
std::string Read3dsCString(std::ifstream &file, std::streampos endPos) {
  std::string output; char ch = 0;
  while (file.tellg() < endPos && file.read(&ch, 1)) {
    if (ch == '\0') break;
    output.push_back(ch);
  }
  return output;
}

// Collects bitmap filenames referenced by standard 3DS material map chunks.
std::vector<std::string> Collect3dsTextureReferences(const fs::path &modelPath) {
  std::vector<std::string> references;
  std::ifstream file(modelPath, std::ios::binary);
  ThreeDsChunkHeader root;
  if (!file.is_open() || !Read3dsChunkHeader(file, root) || root.id != 0x4D4D)
    return references;
  std::unordered_set<std::string> seen;
  const std::streampos rootEnd = static_cast<std::streampos>(root.length);
  while (file.tellg() < rootEnd) {
    ThreeDsChunkHeader chunk;
    if (!Read3dsChunkHeader(file, chunk) || chunk.length < 6) break;
    const auto chunkEnd = file.tellg() + static_cast<std::streamoff>(chunk.length - 6);
    if (chunk.id != 0x3D3D) { file.seekg(chunkEnd); continue; }
    while (file.tellg() < chunkEnd) {
      ThreeDsChunkHeader sub;
      if (!Read3dsChunkHeader(file, sub) || sub.length < 6) break;
      const auto subEnd = file.tellg() + static_cast<std::streamoff>(sub.length - 6);
      if (sub.id != 0xAFFF) { file.seekg(subEnd); continue; }
      while (file.tellg() < subEnd) {
        ThreeDsChunkHeader material;
        if (!Read3dsChunkHeader(file, material) || material.length < 6) break;
        const auto materialEnd = file.tellg() + static_cast<std::streamoff>(material.length - 6);
        if (material.id != 0xA200) { file.seekg(materialEnd); continue; }
        while (file.tellg() < materialEnd) {
          ThreeDsChunkHeader texture;
          if (!Read3dsChunkHeader(file, texture) || texture.length < 6) break;
          const auto textureEnd = file.tellg() + static_cast<std::streamoff>(texture.length - 6);
          if (texture.id == 0xA300) {
            const std::string value = Read3dsCString(file, textureEnd);
            if (!value.empty() && seen.insert(ToLowerAscii(value)).second)
              references.push_back(value);
          }
          file.seekg(textureEnd);
        }
      }
    }
  }
  return references;
}

// Collects local external URIs from glTF JSON or a GLB JSON chunk.
std::vector<std::string> CollectGltfExternalReferences(const fs::path &modelPath) {
  std::vector<std::string> references;
  std::ifstream file(modelPath, std::ios::binary);
  if (!file.is_open()) return references;
  std::ostringstream content; content << file.rdbuf();
  std::string jsonText = content.str();
  if (ToLowerAscii(modelPath.extension().string()) == ".glb") {
    const auto readUint32 = [&](size_t offset) {
      const auto *b = reinterpret_cast<const unsigned char *>(jsonText.data());
      return static_cast<uint32_t>(b[offset]) | (static_cast<uint32_t>(b[offset + 1]) << 8) |
             (static_cast<uint32_t>(b[offset + 2]) << 16) | (static_cast<uint32_t>(b[offset + 3]) << 24);
    };
    if (jsonText.size() < 20 || readUint32(0) != 0x46546C67 ||
        readUint32(16) != 0x4E4F534A || readUint32(12) > jsonText.size() - 20)
      return references;
    jsonText = jsonText.substr(20, readUint32(12));
  }
  std::unordered_set<std::string> seen;
  const std::regex uriRegex(R"re("uri"\s*:\s*"([^"]+)")re");
  for (std::sregex_iterator it(jsonText.begin(), jsonText.end(), uriRegex), end; it != end; ++it) {
    std::string uri = (*it)[1].str();
    const std::string lower = ToLowerAscii(TrimAscii(uri));
    if (lower.empty() || lower.rfind("data:", 0) == 0 ||
        lower.rfind("http://", 0) == 0 || lower.rfind("https://", 0) == 0)
      continue;
    if (seen.insert(ToLowerAscii(uri)).second) references.push_back(std::move(uri));
  }
  return references;
}

// Resolves a local model dependency, including the existing case-insensitive fallback.
bool ResolveModelDependencyPath(const fs::path &modelPath,
                                const std::string &reference, fs::path &resolved) {
  const fs::path ref = PathUtils::PathFromUtf8(TrimAscii(reference));
  if (ref.empty()) return false;
  if (ref.is_absolute() && fs::exists(ref)) { resolved = ref; return true; }
  const fs::path directory = modelPath.parent_path();
  if (directory.empty()) return false;
  const fs::path direct = directory / ref;
  if (fs::exists(direct)) { resolved = direct; return true; }
  std::error_code ec;
  for (const auto &entry : fs::directory_iterator(directory, fs::directory_options::skip_permission_denied, ec)) {
    if (ec) break;
    if (entry.is_regular_file() && ToLowerAscii(entry.path().filename().string()) ==
                                       ToLowerAscii(ref.filename().string())) {
      resolved = entry.path(); return true;
    }
  }
  return false;
}

// Locates the established dummy fixture fallback without changing preference order.
std::string ResolveFallbackFixtureGdtfPath() {
  static const std::string resolved = [] {
    const fs::path base = ProjectUtils::GetBaseLibraryPath("fixtures");
    const std::array<fs::path, 5> candidates = {
        base / "Dummy 1ch.gdtf",
        base / "Perastage@Dummy_1ch@Perastage.gdtf",
        base / "Unknown@Dummy_1ch@Perastage.gdtf",
        base / "Generic 1ch.gdtf",
        base / "Generic@Generic_1ch@Perastage.gdtf"};
    for (const fs::path &path : candidates) {
      std::error_code ec;
      if (fs::exists(path, ec) && !ec && fs::is_regular_file(path, ec) && !ec)
        return path.generic_string();
    }
    return std::string{};
  }();
  return resolved;
}

} // namespace

namespace mvr_export_resources {

// Initializes collection and reserves a temporary workspace for primitive models.
ResourceCollection::ResourceCollection(std::string sceneBasePath,
                                       DiagnosticSink diagnosticSink,
                                       InformationalLogSink informationalLogSink)
    : m_sceneBasePath(std::move(sceneBasePath)),
      m_diagnosticSink(std::move(diagnosticSink)),
      m_informationalLogSink(std::move(informationalLogSink)) {
  runtime_storage::TemporaryWorkspace workspace("mvr-export-primitives");
  if (workspace.IsValid()) {
    m_primitiveWorkspace = workspace.Path();
    m_workspaceLeases.push_back(workspace.TransferToSceneLease());
    m_plan.workspaceLeases.push_back(m_workspaceLeases.back());
  }
}

// Removes generated files after archive packaging has completed.
ResourceCollection::~ResourceCollection() {
  for (const fs::path &path : m_generatedPaths)
    runtime_storage::RemoveOwnedPath(path, "MVR export generated file");
}

// Sanitizes arbitrary input into one portable root-level archive filename.
std::string ResourceCollection::SanitizeArchiveFileName(
    const std::string &input, const std::string &fallback) {
  std::string candidate = TrimAscii(input);
  std::replace(candidate.begin(), candidate.end(), '\\', '/');
  std::string value = fs::path(candidate).filename().generic_string();
  if (value.empty()) value = fallback;
  for (char &ch : value) {
    const unsigned char uch = static_cast<unsigned char>(ch);
    if (uch < 0x20 || ch == '<' || ch == '>' || ch == ':' || ch == '"' ||
        ch == '/' || ch == '\\' || ch == '|' || ch == '?' || ch == '*') ch = '_';
  }
  if (value.empty()) value = fallback;
  return TruncateFileNamePreservingExtension(value, 120);
}

// Normalizes archive separators and removes leading relative markers.
std::string ResourceCollection::NormalizeArchiveEntryPath(std::string path) {
  std::replace(path.begin(), path.end(), '\\', '/');
  while (path.rfind("./", 0) == 0) path.erase(0, 2);
  while (!path.empty() && path.front() == '/') path.erase(path.begin());
  return path;
}

// Resolves the established compatibility fixture GDTF in preference order.
std::string ResourceCollection::ResolveFallbackFixtureGdtfPath() {
  return ::ResolveFallbackFixtureGdtfPath();
}

// Produces a stable absolute identity path relative to the scene base directory.
std::string ResourceCollection::NormalizeSourcePath(const std::string &rawPath) const {
  fs::path source = PathUtils::PathFromUtf8(rawPath);
  if (source.is_relative() && !m_sceneBasePath.empty())
    source = PathUtils::PathFromUtf8(m_sceneBasePath) / source;
  std::error_code ec;
  const fs::path weak = fs::weakly_canonical(source, ec);
  if (!ec) return PathUtils::PathToUtf8(weak);
  ec.clear();
  const fs::path absolute = fs::absolute(source, ec);
  return PathUtils::PathToUtf8(ec ? source.lexically_normal() : absolute.lexically_normal());
}

// Resolves an existing source directly or by filename within scene resources.
std::string ResourceCollection::ResolveExistingResourceSourcePath(const std::string &rawSource) const {
  if (rawSource.empty()) return {};
  std::string normalized = NormalizeSourcePath(rawSource);
  std::error_code ec;
  if (fs::exists(PathUtils::PathFromUtf8(normalized), ec) && !ec) return normalized;
  if (m_sceneBasePath.empty()) return {};
  const fs::path requested = PathUtils::PathFromUtf8(rawSource).filename();
  const fs::path base = PathUtils::PathFromUtf8(m_sceneBasePath);
  if (requested.empty() || !fs::exists(base, ec) || ec) return {};
  for (const auto &entry : fs::directory_iterator(base, ec)) {
    if (ec) break;
    std::error_code regularEc;
    if (entry.is_regular_file(regularEc) && !regularEc &&
        ToLowerAscii(entry.path().filename().generic_string()) ==
            ToLowerAscii(requested.generic_string())) {
      normalized = NormalizeSourcePath(entry.path().generic_string());
      if (m_informationalLogSink)
        m_informationalLogSink("MVR export resolved packaged resource '" + rawSource +
                               "' by filename in scene resources: " + normalized);
      return normalized;
    }
  }
  return {};
}

// Resolves an existing scene resource path for orchestration-only decisions.
std::string ResourceCollection::ResolveSourcePath(const std::string &rawSource) const {
  return ResolveExistingResourceSourcePath(rawSource);
}

// Builds the same filesystem identity used for deterministic source reuse.
std::string ResourceCollection::BuildSourceIdentity(const std::string &sourcePath) const {
  return PathUtils::BuildFilesystemIdentityKey(
      PathUtils::PathFromUtf8(NormalizeSourcePath(sourcePath)));
}

// Registers one deterministic archive entry and reports missing resources.
std::string ResourceCollection::RegisterResource(const std::string &rawSource,
    const std::string &preferredArchivePath, ResourceKind kind,
    ResourceProvenance provenance, bool allowReuseBySource) {
  if (rawSource.empty()) return {};
  std::string normalized = ResolveExistingResourceSourcePath(rawSource);
  if (normalized.empty()) normalized = NormalizeSourcePath(rawSource);
  const std::string identity = PathUtils::BuildFilesystemIdentityKey(PathUtils::PathFromUtf8(normalized));
  const auto existing = m_sourceToArchivePath.find(identity);
  if (allowReuseBySource && existing != m_sourceToArchivePath.end()) return existing->second;
  const std::string archive = EnsureUniqueArchivePath(preferredArchivePath, m_reservedArchivePaths);
  if (allowReuseBySource) m_sourceToArchivePath[identity] = archive;
  m_plan.entries.push_back({PathUtils::PathFromUtf8(normalized), archive, kind, provenance});
  if (!fs::exists(PathUtils::PathFromUtf8(normalized)) && m_diagnosticSink)
    m_diagnosticSink({MvrExportDiagnosticCode::ResourceMissing, MvrExportDiagnosticSeverity::Warning,
      MvrExportDiagnosticImpact::DataOmitted, true, {}, {}, {},
      fs::path(preferredArchivePath).filename().generic_string(),
      "Referenced MVR resource could not be found and will be omitted: " +
      fs::path(preferredArchivePath).filename().generic_string()});
  return archive;
}

// Selects a GDTF source, explicitly classifying fallback substitution.
std::string ResourceCollection::RegisterGdtfResource(const std::string &objectUuid,
    const std::string &rawGdtfPath, const std::string &preferredName,
    bool allowReuseBySource, bool usePreferredDerivativeName, bool allowFallback) {
  if (rawGdtfPath.empty()) return {};
  std::string resolved = ResolveExistingResourceSourcePath(rawGdtfPath);
  ResourceProvenance provenance = ResourceProvenance::StandardPreserved;
  if (resolved.empty() && allowFallback) {
    resolved = ResolveFallbackFixtureGdtfPath();
    if (!resolved.empty()) {
      provenance = ResourceProvenance::CompatibilityFallback;
      if (m_diagnosticSink) m_diagnosticSink({MvrExportDiagnosticCode::GdtfFallbackUsed,
        MvrExportDiagnosticSeverity::Warning, MvrExportDiagnosticImpact::DataSubstituted,
        true, "Fixture", {}, objectUuid, fs::path(rawGdtfPath).filename().generic_string(),
        "MVR export could not resolve fixture GDTF '" + rawGdtfPath + "'. Using fallback '" +
        fs::path(resolved).filename().generic_string() + "'."});
    }
  }
  if (resolved.empty() && !allowFallback) {
    if (m_diagnosticSink) m_diagnosticSink({MvrExportDiagnosticCode::TrussGdtfMissing,
      MvrExportDiagnosticSeverity::Warning, MvrExportDiagnosticImpact::DataOmitted,
      true, "Truss", {}, objectUuid, fs::path(rawGdtfPath).filename().generic_string(),
      "MVR export omitted missing explicit auxiliary Truss GDTF '" + rawGdtfPath +
      "'; no fallback was substituted."});
    return {};
  }
  const std::string source = resolved.empty() ? rawGdtfPath : resolved;
  std::string fileName = preferredName;
  if (!usePreferredDerivativeName && ToLowerAscii(PathUtils::PathFromUtf8(rawGdtfPath).extension().string()) == ".gdtf" &&
      !GdtfDictionary::IsPerastageNamedGdtfFile(rawGdtfPath))
    fileName = GdtfDictionary::BuildPerastageCanonicalGdtfFileName(source);
  if (fileName.empty()) fileName = SanitizeArchiveFileName(rawGdtfPath, "fixture.gdtf");
  const std::string archive = RegisterResource(source, fileName, ResourceKind::Gdtf, provenance, allowReuseBySource);
  if (!objectUuid.empty() && !archive.empty()) m_plan.gdtfArchiveByObjectUuid[objectUuid] = archive;
  return archive;
}

// Registers a model and discovers all supported external dependencies.
std::string ResourceCollection::RegisterModelResource(const std::string &rawModelSource,
                                                       const std::string &fallbackArchiveName) {
  const std::string resolved = ResolveExistingResourceSourcePath(rawModelSource);
  const std::string source = resolved.empty() ? rawModelSource : resolved;
  const std::string archive = RegisterResource(source, SanitizeArchiveFileName(rawModelSource, fallbackArchiveName));
  RegisterModelDependencies(resolved, archive);
  return archive;
}

// Discovers and registers 3DS, glTF, and GLB external resource dependencies.
void ResourceCollection::RegisterModelDependencies(const std::string &resolvedSource,
                                                   const std::string &modelArchivePath) {
  if (resolvedSource.empty() || modelArchivePath.empty()) return;
  const fs::path modelPath = PathUtils::PathFromUtf8(resolvedSource);
  const std::string extension = ToLowerAscii(modelPath.extension().string());
  std::vector<std::string> references;
  if (extension == ".3ds") references = Collect3dsTextureReferences(modelPath);
  else if (extension == ".gltf" || extension == ".glb") references = CollectGltfExternalReferences(modelPath);
  for (const std::string &reference : references) {
    fs::path dependency;
    if (!ResolveModelDependencyPath(modelPath, reference, dependency)) {
      if (m_diagnosticSink) m_diagnosticSink({MvrExportDiagnosticCode::TextureMissing,
        MvrExportDiagnosticSeverity::Warning, MvrExportDiagnosticImpact::DataOmitted,
        true, "Model", {}, {}, fs::path(reference).filename().generic_string(),
        "A required external model dependency could not be found: " + fs::path(reference).filename().generic_string()});
      continue;
    }
    std::string normalized = TrimAscii(reference); std::replace(normalized.begin(), normalized.end(), '\\', '/');
    const std::string archive = fs::path(normalized).filename().generic_string();
    if (SanitizeArchiveFileName(archive, dependency.filename().generic_string()) != archive) {
      m_dependencyRegistrationFailed = true;
      if (m_diagnosticSink) m_diagnosticSink({MvrExportDiagnosticCode::StructuralValidationFailed,
        MvrExportDiagnosticSeverity::Error, MvrExportDiagnosticImpact::ExportFailed,
        true, "Model", {}, {}, archive,
        "MVR export cannot preserve external model dependency URI '" + reference + "' as a root archive filename."});
      continue;
    }
    const std::string identity = PathUtils::BuildFilesystemIdentityKey(dependency);
    const auto collision = std::find_if(m_plan.entries.begin(), m_plan.entries.end(), [&](const ResourceEntry &entry) {
      return ToLowerAscii(entry.archivePath) == ToLowerAscii(archive);
    });
    if (collision != m_plan.entries.end() &&
        PathUtils::BuildFilesystemIdentityKey(collision->sourcePath) != identity) {
      m_dependencyRegistrationFailed = true;
      if (m_diagnosticSink) m_diagnosticSink({MvrExportDiagnosticCode::StructuralValidationFailed,
        MvrExportDiagnosticSeverity::Error, MvrExportDiagnosticImpact::ExportFailed,
        true, "Model", {}, {}, archive,
        "MVR export found conflicting resources for required model dependency '" + archive + "'."});
      continue;
    }
    if (collision == m_plan.entries.end()) {
      m_reservedArchivePaths.insert(archive);
      m_sourceToArchivePath.try_emplace(identity, archive);
      m_plan.entries.push_back({dependency, archive, ResourceKind::ModelDependency,
                                ResourceProvenance::StandardPreserved});
    }
    m_plan.modelDependenciesByArchivePath[modelArchivePath].insert(archive);
  }
}

// Generates or reuses a deterministic primitive GLB resource.
std::string ResourceCollection::RegisterPrimitiveModelResource(const std::string &modelRef,
                                                                const std::string &objectUuid) {
  std::string token;
  if (!mvr::ResolvePrimitiveTokenFromModelRef(modelRef, token) || m_primitiveWorkspace.empty()) return {};
  std::string key = ToLowerAscii(TrimAscii(modelRef));
  if (key.empty()) key = token;
  if (key.rfind("primitive:cylinder", 0) == 0) {
    const size_t separator = key.find(';');
    if (separator != std::string::npos) {
      std::stringstream input(key.substr(separator + 1)); std::string field; std::string converted = "primitive:cylinder";
      while (std::getline(input, field, ';')) {
        const size_t equal = field.find('=');
        if (equal != std::string::npos && (field.substr(0, equal) == "top" || field.substr(0, equal) == "bottom" || field.substr(0, equal) == "height")) {
          try { field = field.substr(0, equal + 1) + std::to_string(std::stof(field.substr(equal + 1)) / 1000.0f); } catch (...) {}
        }
        if (!field.empty()) converted += ";" + field;
      }
      key = converted;
    }
  }
  std::string source;
  const auto existing = m_primitiveSourceByToken.find(key);
  if (existing != m_primitiveSourceByToken.end()) source = existing->second;
  else {
    std::string label = token.substr(token.find(':') == std::string::npos ? 0 : token.find(':') + 1);
    for (char &ch : label) if (!std::isalnum(static_cast<unsigned char>(ch))) ch = '_';
    if (label.empty()) label = "shape";
    const fs::path output = m_primitiveWorkspace /
      ("primitive_" + label + "_" + [&] { std::ostringstream out; out << std::hex << std::hash<std::string>{}(key); return out.str(); }() + ".glb");
    if (!mvr::WritePrimitiveModelForToken(key, output.generic_string())) return {};
    source = output.generic_string(); m_primitiveSourceByToken[key] = source;
  }
  return RegisterResource(source, mvr::PrimitiveArchivePathForToken(token, objectUuid),
                          ResourceKind::PrimitiveModel, ResourceProvenance::CompatibilityFallback);
}

// Retains ownership of an orchestration-generated resource until packaging ends.
void ResourceCollection::AdoptGeneratedResource(const fs::path &path, ResourceProvenance) {
  m_generatedPaths.push_back(path);
  m_plan.generatedPaths.push_back(path);
}

// Retains a generated-resource workspace until archive packaging completes.
void ResourceCollection::AdoptWorkspace(runtime_storage::TemporaryWorkspace workspace) {
  if (!workspace.IsValid())
    return;
  m_workspaceLeases.push_back(workspace.TransferToSceneLease());
  m_plan.workspaceLeases.push_back(m_workspaceLeases.back());
}

// Associates an object with a previously registered GDTF archive.
void ResourceCollection::AssociateGdtfArchive(const std::string &objectUuid,
                                               const std::string &archivePath) {
  if (!objectUuid.empty() && !archivePath.empty())
    m_plan.gdtfArchiveByObjectUuid[objectUuid] = archivePath;
}

// Prunes unreferenced entries, closes model dependencies, and deduplicates paths.
ResourcePlan ResourceCollection::Finalize(const std::unordered_set<std::string> &referencedPaths) {
  std::unordered_set<std::string> closure = referencedPaths;
  for (const auto &[model, dependencies] : m_plan.modelDependenciesByArchivePath) {
    if (!closure.contains(NormalizeArchiveEntryPath(model)))
      continue;
    for (const std::string &dependency : dependencies)
      closure.insert(NormalizeArchiveEntryPath(dependency));
  }

  const std::size_t before = m_plan.entries.size();
  std::vector<ResourceEntry> referenced;
  for (const ResourceEntry &entry : m_plan.entries) {
    const std::string normalized = NormalizeArchiveEntryPath(entry.archivePath);
    if (normalized.empty() || !closure.contains(normalized)) {
      if (!normalized.empty() && m_informationalLogSink)
        m_informationalLogSink(
            "MVR export pruned unreferenced archive resource: " + normalized);
      continue;
    }
    referenced.push_back(entry);
  }
  if (m_informationalLogSink)
    m_informationalLogSink(
        "MVR export resource pruning summary: referenced_paths=" +
        std::to_string(closure.size()) + ", planned_resources_before=" +
        std::to_string(before) + ", planned_resources_after=" +
        std::to_string(referenced.size()));

  std::unordered_set<std::string> seen;
  std::vector<ResourceEntry> deduplicated;
  deduplicated.reserve(referenced.size());
  for (const ResourceEntry &entry : referenced) {
    const std::string normalized = NormalizeArchiveEntryPath(entry.archivePath);
    if (!seen.insert(normalized).second) {
      if (m_diagnosticSink)
        m_diagnosticSink({MvrExportDiagnosticCode::ResourceDuplicate,
          MvrExportDiagnosticSeverity::Warning,
          MvrExportDiagnosticImpact::DataOmitted, true, {}, {}, {},
          fs::path(normalized).filename().generic_string(),
          "Referenced file '" + normalized +
              "' appears multiple times; duplicates will be ignored."});
      continue;
    }
    deduplicated.push_back(entry);
  }
  m_plan.entries = std::move(deduplicated);
  return m_plan;
}

// Returns object-to-GDTF archive references resolved during collection.
const std::unordered_map<std::string, std::string> &ResourceCollection::GdtfArchiveByObjectUuid() const {
  return m_plan.gdtfArchiveByObjectUuid;
}

// Reports whether dependency registration found an archive-breaking conflict.
bool ResourceCollection::HasFatalDependencyError() const { return m_dependencyRegistrationFailed; }

} // namespace mvr_export_resources
