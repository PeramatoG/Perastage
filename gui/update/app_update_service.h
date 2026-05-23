#pragma once

#include <string>

namespace gui::update {

// Represents the high-level outcome of checking whether a newer app release exists.
enum class CheckStatus { UpToDate, UpdateAvailable, CheckFailed };

// Carries the result payload for the update check including versions and links.
struct CheckResult {
  CheckStatus status = CheckStatus::CheckFailed;
  std::string currentVersion;
  std::string latestVersion;
  std::string releaseUrl;
  std::string changelogUrl;
  std::string errorMessage;
};

// Provides app-update lookup and semantic-version comparison against GitHub releases.
class AppUpdateService {
public:
  // Creates a service configured to query the default Perastage release feed.
  AppUpdateService();

  // Executes the full update check and returns status, versions, URLs, and errors.
  CheckResult CheckForUpdates() const;

private:
  std::string releasesApiUrl;
  std::string releasesPageUrl;

  // Loads the currently running application version string.
  std::string ReadCurrentVersion() const;

  // Fetches and parses the latest stable release metadata from GitHub.
  bool QueryLatestStableRelease(std::string &versionOut, std::string &releaseUrlOut,
                                std::string &changelogUrlOut,
                                std::string &errorOut) const;

  // Compares two semantic versions and returns -1, 0, or 1.
  static int CompareSemanticVersions(const std::string &left,
                                     const std::string &right);

  // Compares only major/minor components and ignores patch-level differences.
  static int CompareMajorMinorVersions(const std::string &left,
                                       const std::string &right);
};

} // namespace gui::update
