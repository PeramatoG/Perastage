#include "wx_path_utils.h"

#include <cassert>
#include <filesystem>
#include <string>
#include <vector>

// Verifies explicit filesystem and wxString path conversion for Unicode paths.
int main() {
  const std::vector<std::string> names = {
      "ascii.gdtf", "path with spaces.gdtf", "Iluminación.gdtf", "照明.gdtf"};
  for (const auto &name : names) {
    std::filesystem::path path = std::filesystem::temp_directory_path() / name;
    const wxString wxPath = WxPathUtils::WxStringFromFilesystemPath(path);
    const std::filesystem::path roundTrip = WxPathUtils::FilesystemPathFromWxString(wxPath);
    assert(roundTrip == path);
  }
  assert(WxPathUtils::WxStringFromFilesystemPath({}).empty());
  assert(WxPathUtils::FilesystemPathFromWxString(wxString()).empty());
  return 0;
}
