#pragma once

#include "filesystem_path_utils.h"
#include "mvrscene.h"
#include "project_archive_resource.h"
#include "project_cache_validation.h"
#include "symbols/project_fixture_symbols.h"

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

// Keeps the currently open project's extracted assets alive until restore commits.
// Failed callbacks or authoritative-resource validation roll back session ownership.
class ProjectResourceLoadTransaction {
public:
  ProjectResourceLoadTransaction(
      std::string &directory, std::vector<ProjectArchiveResource> &resources,
      project_cache::ValidationContext &validation, MvrScene &scene,
      symbols::ProjectFixtureSymbolStore &symbols)
      : directory_(directory), resources_(resources), validation_(validation),
        scene_(scene), symbols_(symbols), previousDirectory_(std::move(directory)),
        previousResources_(std::move(resources)), previousValidation_(validation),
        previousScene_(scene), previousSymbols_(symbols) {
    directory_.clear();
    resources_.clear();
    validation_ = {};
  }

  ~ProjectResourceLoadTransaction() {
    if (committed_)
      return;
    RemoveDirectory(directory_);
    directory_ = std::move(previousDirectory_);
    resources_ = std::move(previousResources_);
    validation_ = std::move(previousValidation_);
    scene_ = std::move(previousScene_);
    symbols_ = std::move(previousSymbols_);
  }

  void Commit() {
    RemoveDirectory(previousDirectory_);
    committed_ = true;
  }

private:
  static void RemoveDirectory(const std::string &directory) {
    if (directory.empty())
      return;
    std::error_code error;
    std::filesystem::remove_all(PathUtils::PathFromUtf8(directory), error);
  }

  std::string &directory_;
  std::vector<ProjectArchiveResource> &resources_;
  project_cache::ValidationContext &validation_;
  MvrScene &scene_;
  symbols::ProjectFixtureSymbolStore &symbols_;
  std::string previousDirectory_;
  std::vector<ProjectArchiveResource> previousResources_;
  project_cache::ValidationContext previousValidation_;
  MvrScene previousScene_;
  symbols::ProjectFixtureSymbolStore previousSymbols_;
  bool committed_ = false;
};
