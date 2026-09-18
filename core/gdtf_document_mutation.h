#pragma once

#include <functional>
#include <string>
#include <vector>

namespace gdtf {

struct DocumentMutationResult {
  bool success = false;
  bool changed = false;
  std::vector<std::string> warnings;
  std::vector<std::string> errors;
  std::string publicationPath;
  bool atomicReplacementCompleted = false;
};

struct DocumentMutationPublicationHooks {
  std::function<bool(const std::string &stage, std::string &error)> beforeStage;
};

struct DocumentMutationRequest {
  bool descriptionSet = false;
  std::string description;
  bool weightSet = false;
  float weightKg = 0.0f;
  bool powerSet = false;
  float powerW = 0.0f;
  std::string revisionText;
};

// Mutates and atomically publishes selected FixtureType document fields.
DocumentMutationResult MutateDocument(
    const std::string &gdtfPath, const DocumentMutationRequest &request,
    const std::string &modifiedByProgram,
    const DocumentMutationPublicationHooks *publicationHooks = nullptr);

} // namespace gdtf
