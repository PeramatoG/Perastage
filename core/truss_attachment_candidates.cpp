#include "truss_attachment_candidates.h"

#include "gdtf_archive_reader.h"
#include "matrixutils.h"

#include <algorithm>
#include <cmath>
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
                     const Matrix &ownerTransform, std::string sourceId) {
  Candidate candidate;
  candidate.stableId = std::move(stableId);
  candidate.kind = kind;
  candidate.localTransform = local;
  candidate.worldTransform = MatrixUtils::Multiply(ownerTransform, local);
  candidate.sourcePath = std::move(sourceId);
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
  if (!archive.Success())
    return {};
  return ReadExplicitGdtfMagnets(archive.descriptionXml, trussTransform);
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
      AppendCandidate(result, CandidateKind::InferredLongitudinalEnd,
                      "longitudinal-axis-" + std::to_string(*dominant) +
                          (sign < 0 ? "-negative" : "-positive"),
                      local, trussTransform, sourceId);
    }
    return result;
  }
  for (int axis = 0; axis < 3; ++axis) {
    for (int sign : {-1, 1}) {
      Matrix local = MatrixUtils::Identity();
      local.o = center;
      local.o[axis] += sign * safeDimensions[axis] * 0.5f;
      AppendCandidate(result, CandidateKind::InferredFaceCenter,
                      "face-axis-" + std::to_string(axis) +
                          (sign < 0 ? "-negative" : "-positive"),
                      local, trussTransform, sourceId);
    }
  }
  return result;
}

// Resolves explicit candidates first and otherwise returns inferred candidates.
std::vector<Candidate> BuildCandidates(const Truss &truss,
                                       std::vector<Diagnostic> *diagnostics) {
  ExplicitReadResult explicitResult =
      ReadExplicitGdtfMagnetsFromArchive(truss.gdtfSpec, truss.transform);
  if (diagnostics)
    *diagnostics = explicitResult.diagnostics;
  if (!explicitResult.candidates.empty())
    return explicitResult.candidates;
  return BuildInferredCandidates(
      {truss.lengthMm, truss.widthMm, truss.heightMm}, truss.transform,
      truss.uuid);
}

} // namespace truss_attachment
