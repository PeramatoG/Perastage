#include "apppaths.h"
#include "filesystem_path_utils.h"

#include <cstdlib>
#include <system_error>

#include <wx/stdpaths.h>

namespace {

// Converts a wxString into a UTF-8 filesystem path.
std::filesystem::path WxStringToPath(const wxString &value) {
  const wxScopedCharBuffer utf8 = value.ToUTF8();
  if (utf8)
    return PathUtils::PathFromUtf8(std::string(utf8.data(), utf8.length()));
  return std::filesystem::path(value.ToStdString());
}

// Returns the platform-preferred per-user app-data path for Perastage.
std::filesystem::path ResolvePreferredUserDataDir() {
#ifdef _WIN32
  if (const char *localAppData = std::getenv("LOCALAPPDATA")) {
    if (*localAppData)
      return PathUtils::PathFromUtf8(localAppData) / "Perastage";
  }
#endif
  const wxString userDataDir = wxStandardPaths::Get().GetUserDataDir();
  return WxStringToPath(userDataDir);
}

} // namespace

namespace AppPaths {

// Returns the primary writable per-user application-data directory.
std::filesystem::path GetUserDataDir() { return ResolvePreferredUserDataDir(); }

// Returns the per-user fallback application-data directory inside the system temp folder.
std::filesystem::path GetUserDataTempFallbackDir() {
  const wxString tempDir = wxStandardPaths::Get().GetTempDir();
  return WxStringToPath(tempDir) / "Perastage";
}

// Returns the full path to the per-user user_config.json file.
std::filesystem::path GetUserConfigFilePath() {
  return GetUserDataDir() / "user_config.json";
}

// Returns the full path to the per-user application log file.
std::filesystem::path GetLogFilePath() { return GetUserDataDir() / "perastage.log"; }

// Ensures the directory exists and reports whether it is available for use.
bool EnsureDirectory(const std::filesystem::path &dir) {
  if (dir.empty())
    return false;
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  return !ec;
}

} // namespace AppPaths
