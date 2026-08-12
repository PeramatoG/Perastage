#pragma once

#include "mvrscene.h"

#include <array>
#include <cstddef>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace truss_attachment {

constexpr float kLongitudinalDominanceRatio = 2.0f;

enum class CandidateKind {
  ExplicitGdtfMagnet,
  InferredLongitudinalEnd,
  InferredFaceCenter
};

struct Candidate {
  std::string stableId;
  std::string ownerTrussUuid;
  CandidateKind kind = CandidateKind::InferredFaceCenter;
  std::string name;
  std::string model;
  Matrix localTransform{};
  Matrix worldTransform{};
  std::optional<std::array<float, 3>> localDirection;
  std::optional<std::array<float, 3>> worldDirection;
  std::string sourcePath;
};

struct Diagnostic {
  std::string path;
  std::string message;
};

struct ExplicitReadResult {
  std::vector<Candidate> candidates;
  std::vector<Diagnostic> diagnostics;
  bool sourceReadable = true;
};

struct CandidateResolution {
  std::vector<Candidate> candidates;
  std::vector<Diagnostic> diagnostics;
  std::string resolvedSourceIdentity;
};

// Caches immutable local attachment definitions by resolved archive version.
class CandidateResolver {
public:
  struct CacheEntry {
    std::vector<Candidate> localCandidates;
    std::vector<Diagnostic> diagnostics;
    std::string resolvedSourceIdentity;
    bool sourceReadable = true;
  };

  CandidateResolution Resolve(const MvrScene &scene, const Truss &truss);
  void Clear();
  std::size_t ArchiveParseCount() const;

private:
  std::map<std::string, CacheEntry> m_cache;
  std::size_t m_archiveParseCount = 0;
};

// Reads Magnet nodes from immutable GDTF description XML.
ExplicitReadResult ReadExplicitGdtfMagnets(const std::string &descriptionXml,
                                           const Matrix &trussTransform);

// Reads Magnet nodes from a GDTF archive without modifying it.
ExplicitReadResult
ReadExplicitGdtfMagnetsFromArchive(const std::string &archivePath,
                                   const Matrix &trussTransform);

// Returns the clearly dominant local dimension, if one exists.
std::optional<int>
ClassifyLongitudinalAxis(const std::array<float, 3> &dimensionsMm);

// Builds inferred terminal or face-center candidates from local bounds.
std::vector<Candidate>
BuildInferredCandidates(const std::array<float, 3> &dimensionsMm,
                        const Matrix &trussTransform,
                        const std::string &sourceId = {});

// Builds inferred candidates from measured local minimum and maximum planes.
std::vector<Candidate>
BuildInferredCandidatesFromBounds(const GeometryBounds &bounds,
                                  const Matrix &trussTransform,
                                  const std::string &sourceId = {});

// Builds exactly six deterministic face-center candidates.
std::vector<Candidate>
BuildAmbiguousCandidates(const std::array<float, 3> &dimensionsMm,
                         const Matrix &trussTransform,
                         const std::string &sourceId = {});

// Resolves cached local definitions and applies the current instance transform.
CandidateResolution BuildCandidates(const MvrScene &scene, const Truss &truss,
                                    CandidateResolver &resolver);

} // namespace truss_attachment
