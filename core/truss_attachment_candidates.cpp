#include "truss_attachment_candidates.h"

#include "filesystem_path_utils.h"
#include "gdtf_archive_reader.h"
#include "matrixutils.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <system_error>
#include <tinyxml2.h>

namespace truss_attachment {
namespace {

// Returns a GDTF transform converted from metres to scene millimetres.
Matrix ToMillimetres(Matrix transform) {
  for (float &component : transform.o)
    component *= 1000.0f;
  return transform;
}

// Appends one deterministic candidate at a local transform.
void AppendCandidate(std::vector<Candidate> &out, CandidateKind kind,
                     std::string stableId, const Matrix &local,
                     const Matrix &ownerTransform, std::string sourceId,
                     std::optional<std::array<float, 3>> localDirection) {
  Candidate candidate;
  candidate.stableId = std::move(stableId);
  candidate.kind = kind;
  candidate.ownerTrussUuid = sourceId;
  candidate.localTransform = local;
  candidate.worldTransform = MatrixUtils::Multiply(ownerTransform, local);
  candidate.sourcePath = std::move(sourceId);
  candidate.localDirection = localDirection;
  if (localDirection) {
    std::array<float, 3> direction{};
    for (int component = 0; component < 3; ++component) {
      direction[component] =
          ownerTransform.u[component] * (*localDirection)[0] +
          ownerTransform.v[component] * (*localDirection)[1] +
          ownerTransform.w[component] * (*localDirection)[2];
    }
    const float length =
        std::sqrt(direction[0] * direction[0] + direction[1] * direction[1] +
                  direction[2] * direction[2]);
    if (length > 1e-6f) {
      for (float &value : direction)
        value /= length;
      candidate.worldDirection = direction;
    }
  }
  out.push_back(std::move(candidate));
}

// Traverses a GDTF geometry subtree while composing parent transforms.
void ReadGeometry(const tinyxml2::XMLElement *node, const Matrix &parent,
                  const Matrix &trussTransform, const std::string &path,
                  ExplicitReadResult &result, size_t &magnetIndex) {
  for (const tinyxml2::XMLElement *current = node; current;
       current = current->NextSiblingElement()) {
    const std::string elementName = current->Name() ? current->Name() : "";
    const std::string name =
        current->Attribute("Name") ? current->Attribute("Name") : "";
    const std::string currentPath =
        path + "/" + elementName + (name.empty() ? "" : "[" + name + "]");
    Matrix local = MatrixUtils::Identity();
    const char *position = current->Attribute("Position");
    const bool positionValid =
        !position || MatrixUtils::ParseMatrix(position, local);
    if (elementName == "Magnet" && !positionValid) {
      result.diagnostics.push_back(
          {currentPath, "Magnet Position is malformed and was skipped."});
    } else {
      const Matrix composed = MatrixUtils::Multiply(parent, local);
      if (elementName == "Magnet") {
        Candidate candidate;
        candidate.stableId = "gdtf-magnet-" + std::to_string(magnetIndex++);
        candidate.kind = CandidateKind::ExplicitGdtfMagnet;
        candidate.name = name;
        candidate.model =
            current->Attribute("Model") ? current->Attribute("Model") : "";
        candidate.localTransform = ToMillimetres(composed);
        candidate.worldTransform =
            MatrixUtils::Multiply(trussTransform, candidate.localTransform);
        candidate.sourcePath = currentPath;
        result.candidates.push_back(std::move(candidate));
      }
      ReadGeometry(current->FirstChildElement(), composed, trussTransform,
                   currentPath, result, magnetIndex);
    }
  }
}

} // namespace

// Reads Magnet nodes from immutable GDTF description XML.
ExplicitReadResult ReadExplicitGdtfMagnets(const std::string &descriptionXml,
                                           const Matrix &trussTransform) {
  ExplicitReadResult result;
  tinyxml2::XMLDocument document;
  if (document.Parse(descriptionXml.c_str(), descriptionXml.size()) !=
      tinyxml2::XML_SUCCESS) {
    result.diagnostics.push_back({"/", "GDTF description XML is malformed."});
    return result;
  }
  const auto *fixtureType =
      document.FirstChildElement("GDTF")
          ? document.FirstChildElement("GDTF")->FirstChildElement("FixtureType")
          : nullptr;
  const auto *geometries =
      fixtureType ? fixtureType->FirstChildElement("Geometries") : nullptr;
  size_t index = 0;
  ReadGeometry(geometries ? geometries->FirstChildElement() : nullptr,
               MatrixUtils::Identity(), trussTransform,
               "/GDTF/FixtureType/Geometries", result, index);
  return result;
}

// Reads Magnet nodes from a GDTF archive without modifying it.
ExplicitReadResult
ReadExplicitGdtfMagnetsFromArchive(const std::string &archivePath,
                                   const Matrix &trussTransform) {
  if (archivePath.empty())
    return {};
  const gdtf::ArchiveReadResult archive = gdtf::ReadGdtfArchive(archivePath);
  ExplicitReadResult result;
  if (!archive.Success()) {
    result.sourceReadable = false;
    for (const auto &diagnostic : archive.diagnostics)
      result.diagnostics.push_back({diagnostic.entryPath, diagnostic.message});
    if (result.diagnostics.empty())
      result.diagnostics.push_back(
          {archivePath, "GDTF archive could not be read."});
    return result;
  }
  result = ReadExplicitGdtfMagnets(archive.descriptionXml, trussTransform);
  for (const auto &diagnostic : archive.diagnostics)
    result.diagnostics.push_back({diagnostic.entryPath, diagnostic.message});
  return result;
}

// Returns the clearly dominant local dimension, if one exists.
std::optional<int>
ClassifyLongitudinalAxis(const std::array<float, 3> &dimensionsMm) {
  for (float dimension : dimensionsMm) {
    if (!std::isfinite(dimension) || dimension <= 0.0f)
      return std::nullopt;
  }
  int largest = 0;
  for (int axis = 1; axis < 3; ++axis) {
    if (dimensionsMm[axis] > dimensionsMm[largest])
      largest = axis;
  }
  float secondLargest = 0.0f;
  for (int axis = 0; axis < 3; ++axis) {
    if (axis != largest)
      secondLargest = std::max(secondLargest, dimensionsMm[axis]);
  }
  // Equality is deliberately ambiguous; dominance must be clear.
  return dimensionsMm[largest] / secondLargest > kLongitudinalDominanceRatio
             ? std::optional<int>(largest)
             : std::nullopt;
}

// Builds inferred terminal or face-center candidates from local bounds.
std::vector<Candidate>
BuildInferredCandidates(const std::array<float, 3> &dimensionsMm,
                        const Matrix &trussTransform,
                        const std::string &sourceId) {
  std::vector<Candidate> result;
  std::array<float, 3> safeDimensions = dimensionsMm;
  for (float &dimension : safeDimensions) {
    if (!std::isfinite(dimension) || dimension <= 0.0f)
      dimension = 1.0f;
  }
  const std::array<float, 3> center{safeDimensions[0] * 0.5f, 0.0f,
                                    safeDimensions[2] * 0.5f};
  if (const auto dominant = ClassifyLongitudinalAxis(dimensionsMm)) {
    for (int sign : {-1, 1}) {
      Matrix local = MatrixUtils::Identity();
      local.o = center;
      local.o[*dominant] += sign * safeDimensions[*dominant] * 0.5f;
      std::array<float, 3> direction{};
      direction[*dominant] = static_cast<float>(sign);
      AppendCandidate(result, CandidateKind::InferredLongitudinalEnd,
                      "longitudinal-axis-" + std::to_string(*dominant) +
                          (sign < 0 ? "-negative" : "-positive"),
                      local, trussTransform, sourceId, direction);
    }
    return result;
  }
  return BuildAmbiguousCandidates(safeDimensions, trussTransform, sourceId);
}

// Builds exactly six deterministic face-center candidates.
std::vector<Candidate>
BuildAmbiguousCandidates(const std::array<float, 3> &dimensionsMm,
                         const Matrix &trussTransform,
                         const std::string &sourceId) {
  std::vector<Candidate> result;
  std::array<float, 3> safeDimensions = dimensionsMm;
  for (float &dimension : safeDimensions) {
    if (!std::isfinite(dimension) || dimension <= 0.0f)
      dimension = 1.0f;
  }
  const std::array<float, 3> center{safeDimensions[0] * 0.5f, 0.0f,
                                    safeDimensions[2] * 0.5f};
  for (int axis = 0; axis < 3; ++axis) {
    for (int sign : {-1, 1}) {
      Matrix local = MatrixUtils::Identity();
      local.o = center;
      local.o[axis] += sign * safeDimensions[axis] * 0.5f;
      std::array<float, 3> direction{};
      direction[axis] = static_cast<float>(sign);
      AppendCandidate(result, CandidateKind::InferredFaceCenter,
                      "face-axis-" + std::to_string(axis) +
                          (sign < 0 ? "-negative" : "-positive"),
                      local, trussTransform, sourceId, direction);
    }
  }
  return result;
}

// Resolves one scene resource reference without consulting the working
// directory.
std::filesystem::path ResolveSceneResource(const MvrScene &scene,
                                           const std::string &reference) {
  const auto path = PathUtils::PathFromUtf8(reference);
  if (path.empty() || path.is_absolute())
    return path;
  if (scene.basePath.empty())
    return {};
  return PathUtils::PathFromUtf8(scene.basePath) / path;
}

// Builds the versioned cache identity for an existing archive.
std::string BuildArchiveVersionKey(const std::filesystem::path &path) {
  std::error_code ec;
  const auto size = std::filesystem::file_size(path, ec);
  if (ec)
    return {};
  const auto modified = std::filesystem::last_write_time(path, ec);
  if (ec)
    return {};
  using PortableFileTicks = std::chrono::duration<std::int64_t, std::nano>;
  const std::int64_t modifiedTicks =
      std::chrono::duration_cast<PortableFileTicks>(modified.time_since_epoch())
          .count();
  return PathUtils::BuildFilesystemIdentityKey(path) + "|" +
         std::to_string(static_cast<std::uintmax_t>(size)) + "|" +
         std::to_string(modifiedTicks);
}

// Applies an instance transform to cached local candidate definitions.
CandidateResolution Instantiate(const CandidateResolver::CacheEntry &entry,
                                const Matrix &transform,
                                const std::string &ownerTrussUuid) {
  CandidateResolution result;
  result.candidates = entry.localCandidates;
  result.diagnostics = entry.diagnostics;
  result.resolvedSourceIdentity = entry.resolvedSourceIdentity;
  for (Candidate &candidate : result.candidates) {
    candidate.worldTransform =
        MatrixUtils::Multiply(transform, candidate.localTransform);
    candidate.ownerTrussUuid = ownerTrussUuid;
    if (candidate.localDirection) {
      std::array<float, 3> direction{};
      for (int component = 0; component < 3; ++component) {
        direction[component] =
            transform.u[component] * (*candidate.localDirection)[0] +
            transform.v[component] * (*candidate.localDirection)[1] +
            transform.w[component] * (*candidate.localDirection)[2];
      }
      const float length =
          std::sqrt(direction[0] * direction[0] + direction[1] * direction[1] +
                    direction[2] * direction[2]);
      if (length > 1e-6f) {
        for (float &value : direction)
          value /= length;
        candidate.worldDirection = direction;
      }
    }
  }
  return result;
}

// Resolves prioritized archive sources and caches immutable local definitions.
CandidateResolution CandidateResolver::Resolve(const MvrScene &scene,
                                               const Truss &truss) {
  std::vector<Diagnostic> resolutionDiagnostics;
  for (const std::string *reference :
       {&truss.gdtfSpec, &truss.perastageAuxGdtfArchivePath}) {
    if (reference->empty())
      continue;
    const std::filesystem::path resolved =
        ResolveSceneResource(scene, *reference);
    if (resolved.empty()) {
      resolutionDiagnostics.push_back(
          {*reference, "Relative GDTF resource has no scene base path."});
      continue;
    }
    const std::string versionKey = BuildArchiveVersionKey(resolved);
    if (versionKey.empty()) {
      resolutionDiagnostics.push_back(
          {PathUtils::PathToUtf8(resolved),
           "GDTF resource is missing or unreadable."});
      continue;
    }
    if (const auto found = m_cache.find(versionKey); found != m_cache.end()) {
      CandidateResolution result =
          Instantiate(found->second, truss.transform, truss.uuid);
      result.diagnostics.insert(result.diagnostics.begin(),
                                resolutionDiagnostics.begin(),
                                resolutionDiagnostics.end());
      if (!found->second.sourceReadable) {
        resolutionDiagnostics = std::move(result.diagnostics);
        continue;
      }
      if (result.candidates.empty())
        result.candidates = BuildInferredCandidates(
            {truss.lengthMm, truss.widthMm, truss.heightMm}, truss.transform,
            truss.uuid);
      return result;
    }

    ++m_archiveParseCount;
    const std::string resolvedText = PathUtils::PathToUtf8(resolved);
    ExplicitReadResult explicitResult = ReadExplicitGdtfMagnetsFromArchive(
        resolvedText, MatrixUtils::Identity());
    CacheEntry entry;
    entry.localCandidates = std::move(explicitResult.candidates);
    entry.diagnostics = std::move(explicitResult.diagnostics);
    entry.resolvedSourceIdentity =
        PathUtils::BuildFilesystemIdentityKey(resolved);
    entry.sourceReadable = explicitResult.sourceReadable;
    for (auto cached = m_cache.begin(); cached != m_cache.end();) {
      if (cached->second.resolvedSourceIdentity == entry.resolvedSourceIdentity)
        cached = m_cache.erase(cached);
      else
        ++cached;
    }
    auto [inserted, unused] = m_cache.emplace(versionKey, std::move(entry));
    (void)unused;
    CandidateResolution result =
        Instantiate(inserted->second, truss.transform, truss.uuid);
    result.diagnostics.insert(result.diagnostics.begin(),
                              resolutionDiagnostics.begin(),
                              resolutionDiagnostics.end());
    if (!inserted->second.sourceReadable) {
      resolutionDiagnostics = std::move(result.diagnostics);
      continue;
    }
    if (result.candidates.empty())
      result.candidates = BuildInferredCandidates(
          {truss.lengthMm, truss.widthMm, truss.heightMm}, truss.transform,
          truss.uuid);
    return result;
  }

  CandidateResolution fallback;
  fallback.diagnostics = std::move(resolutionDiagnostics);
  fallback.candidates =
      BuildInferredCandidates({truss.lengthMm, truss.widthMm, truss.heightMm},
                              truss.transform, truss.uuid);
  return fallback;
}

// Clears all cached type-level attachment definitions.
void CandidateResolver::Clear() { m_cache.clear(); }

// Returns the number of archive parses performed by this resolver.
std::size_t CandidateResolver::ArchiveParseCount() const {
  return m_archiveParseCount;
}

// Resolves cached local definitions and applies the current instance transform.
CandidateResolution BuildCandidates(const MvrScene &scene, const Truss &truss,
                                    CandidateResolver &resolver) {
  return resolver.Resolve(scene, truss);
}

} // namespace truss_attachment
