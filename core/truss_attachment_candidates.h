#pragma once

#include "truss.h"

#include <array>
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
  CandidateKind kind = CandidateKind::InferredFaceCenter;
  std::string name;
  std::string model;
  Matrix localTransform{};
  Matrix worldTransform{};
  std::string sourcePath;
};

struct Diagnostic {
  std::string path;
  std::string message;
};

struct ExplicitReadResult {
  std::vector<Candidate> candidates;
  std::vector<Diagnostic> diagnostics;
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

// Resolves explicit candidates first and otherwise returns inferred candidates.
std::vector<Candidate>
BuildCandidates(const Truss &truss,
                std::vector<Diagnostic> *diagnostics = nullptr);

} // namespace truss_attachment
