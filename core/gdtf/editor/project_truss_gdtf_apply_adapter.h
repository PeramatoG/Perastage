#pragma once

#include "gdtf_editor_context.h"
#include "truss.h"

#include <filesystem>
#include <functional>
#include <unordered_map>
#include <optional>
#include <utility>
#include <string>
#include <vector>

namespace gdtf {

struct ProjectTrussGdtfApplyServices {
  std::function<std::string(const std::string &, const std::string &, const std::string &)> canonicalFileName;
  std::function<bool(const Truss &, const std::filesystem::path &, const std::string &, std::string &)> generateGdtf;
};

struct ProjectTrussGdtfApplyInput {
  GdtfApplyRequest request;
  const std::unordered_map<std::string, Truss> *trusses = nullptr;
  std::filesystem::path projectResourceBasePath;
  std::filesystem::path outputRoot;
};

struct ProjectTrussGdtfApplyResult {
  GdtfApplyResult common;
  std::optional<Truss> resultingTruss;
  std::vector<std::pair<std::string, Truss>> resultingTrusses;
  std::string resultingProjectReference;
  std::string canonicalFileName;
  std::vector<std::string> affectedTrussUuids;
  bool generationOccurred = false;
  bool existingOwnedFileUpdated = false;
  bool newFileCreated = false;
  bool externalFileCreatedOrModified = false;
  bool tableResynchronizationRequired = false;
  bool previewRefreshRequired = false;
};

class ProjectTrussGdtfApplyAdapter {
public:
  explicit ProjectTrussGdtfApplyAdapter(ProjectTrussGdtfApplyServices services);
  ProjectTrussGdtfApplyResult Apply(const ProjectTrussGdtfApplyInput &input) const;

private:
  ProjectTrussGdtfApplyServices services_;
};

} // namespace gdtf
