#include "app_update_service.h"

#include "app_version.h"
#include "third_party/json.hpp"

#include <curl/curl.h>

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

namespace gui::update {
namespace {

// Appends received HTTP bytes to a string buffer for libcurl callbacks.
size_t WriteToString(void *contents, size_t size, size_t nmemb, void *userp) {
  const size_t totalBytes = size * nmemb;
  auto *out = static_cast<std::string *>(userp);
  out->append(static_cast<const char *>(contents), totalBytes);
  return totalBytes;
}

// Removes a leading 'v' prefix and surrounding whitespace from a version string.
std::string NormalizeVersionString(const std::string &version) {
  size_t start = 0;
  size_t end = version.size();
  while (start < end && std::isspace(static_cast<unsigned char>(version[start]))) {
    ++start;
  }
  while (end > start && std::isspace(static_cast<unsigned char>(version[end - 1]))) {
    --end;
  }
  std::string normalized = version.substr(start, end - start);
  if (!normalized.empty() && (normalized[0] == 'v' || normalized[0] == 'V')) {
    normalized.erase(0, 1);
  }
  return normalized;
}

// Splits a dotted semantic version string into numeric components.
bool ParseSemVerParts(const std::string &version, std::vector<int> &partsOut) {
  std::stringstream ss(NormalizeVersionString(version));
  std::string segment;
  while (std::getline(ss, segment, '.')) {
    if (segment.empty() ||
        !std::all_of(segment.begin(), segment.end(), [](unsigned char ch) {
          return std::isdigit(ch) != 0;
        })) {
      return false;
    }
    partsOut.push_back(std::stoi(segment));
  }
  return !partsOut.empty();
}

} // namespace

// Initializes update endpoints for release API data and user-facing download pages.
AppUpdateService::AppUpdateService()
    : releasesApiUrl("https://api.github.com/repos/luisma-peramato/perastage/releases"),
      releasesPageUrl("https://github.com/luisma-peramato/perastage/releases") {}

// Runs the update check flow and classifies availability versus failures.
CheckResult AppUpdateService::CheckForUpdates() const {
  CheckResult result;
  result.currentVersion = ReadCurrentVersion();
  result.changelogUrl = releasesPageUrl;

  std::string latestVersion;
  std::string releaseUrl;
  std::string changelogUrl;
  std::string error;
  if (!QueryLatestStableRelease(latestVersion, releaseUrl, changelogUrl, error)) {
    result.status = CheckStatus::CheckFailed;
    result.errorMessage = error.empty() ? "Unable to retrieve release information." : error;
    result.releaseUrl = releasesPageUrl;
    return result;
  }

  result.latestVersion = latestVersion;
  result.releaseUrl = releaseUrl.empty() ? releasesPageUrl : releaseUrl;
  result.changelogUrl = changelogUrl.empty() ? releasesPageUrl : changelogUrl;

  const int compare = CompareSemanticVersions(result.currentVersion, result.latestVersion);
  result.status = compare < 0 ? CheckStatus::UpdateAvailable : CheckStatus::UpToDate;
  return result;
}

// Returns the canonical version string embedded at build time.
std::string AppUpdateService::ReadCurrentVersion() const { return app::kVersion; }

// Retrieves the latest non-draft, non-prerelease GitHub release and extracts its fields.
bool AppUpdateService::QueryLatestStableRelease(std::string &versionOut,
                                                std::string &releaseUrlOut,
                                                std::string &changelogUrlOut,
                                                std::string &errorOut) const {
  CURL *curl = curl_easy_init();
  if (!curl) {
    errorOut = "Unable to initialize network layer.";
    return false;
  }

  std::string responseBody;
  struct curl_slist *headers = nullptr;
  headers = curl_slist_append(headers, "Accept: application/vnd.github+json");
  headers = curl_slist_append(headers, "User-Agent: Perastage-UpdateChecker");

  curl_easy_setopt(curl, CURLOPT_URL, releasesApiUrl.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteToString);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 12L);

  const CURLcode code = curl_easy_perform(curl);
  long httpStatus = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpStatus);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  if (code != CURLE_OK) {
    errorOut = "Network request failed. Please check your connection and try again.";
    return false;
  }
  if (httpStatus == 401 || httpStatus == 403) {
    errorOut = "Release service rejected the request (authorization or rate limit).";
    return false;
  }
  if (httpStatus < 200 || httpStatus >= 300) {
    errorOut = "Release service returned an unexpected response.";
    return false;
  }

  const nlohmann::json parsed = nlohmann::json::parse(responseBody, nullptr, false);
  if (!parsed.is_array()) {
    errorOut = "Release data format was not recognized.";
    return false;
  }

  for (const auto &release : parsed) {
    if (!release.is_object()) {
      continue;
    }
    const bool prerelease = release.value("prerelease", false);
    const bool draft = release.value("draft", false);
    if (prerelease || draft) {
      continue;
    }

    const std::string tag = release.value("tag_name", "");
    const std::string htmlUrl = release.value("html_url", "");
    if (tag.empty()) {
      continue;
    }

    versionOut = NormalizeVersionString(tag);
    releaseUrlOut = htmlUrl;
    changelogUrlOut = htmlUrl;
    return true;
  }

  errorOut = "No stable release was found.";
  return false;
}

// Lexicographically compares numeric semantic-version components.
int AppUpdateService::CompareSemanticVersions(const std::string &left,
                                              const std::string &right) {
  std::vector<int> leftParts;
  std::vector<int> rightParts;
  if (!ParseSemVerParts(left, leftParts) || !ParseSemVerParts(right, rightParts)) {
    return left == right ? 0 : (left < right ? -1 : 1);
  }

  const size_t count = std::max(leftParts.size(), rightParts.size());
  leftParts.resize(count, 0);
  rightParts.resize(count, 0);

  for (size_t i = 0; i < count; ++i) {
    if (leftParts[i] < rightParts[i]) {
      return -1;
    }
    if (leftParts[i] > rightParts[i]) {
      return 1;
    }
  }
  return 0;
}

} // namespace gui::update
