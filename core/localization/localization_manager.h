#pragma once

#include "localization/app_language.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

class wxLocale;

namespace localization {

struct LocalizationInitResult {
  AppLanguage requestedLanguage = AppLanguage::English;
  AppLanguage activeLanguage = AppLanguage::English;
  bool catalogLoaded = false;
  std::string diagnostic;
};

// Returns deterministic locale roots for the current runtime layout.
std::vector<std::filesystem::path> ResolveLocaleRootCandidates();

// Returns deterministic locale roots for a supplied executable path and working directory.
std::vector<std::filesystem::path> ResolveLocaleRootCandidatesForPaths(
    const std::filesystem::path &executablePath,
    const std::filesystem::path &workingDirectory);

class LocalizationManager {
public:
  // Returns the process-wide localization manager owned for application lifetime.
  static LocalizationManager &Get();

  // Initializes wxWidgets catalog loading for the selected UI language.
  LocalizationInitResult Initialize(AppLanguage language);

  // Returns the language currently active in this process.
  AppLanguage ActiveLanguage() const;

private:
  LocalizationManager() = default;

  AppLanguage activeLanguage_ = AppLanguage::English;
  std::unique_ptr<wxLocale> locale_;
};

} // namespace localization
