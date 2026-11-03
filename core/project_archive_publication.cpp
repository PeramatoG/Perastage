#include "project_archive_publication.h"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace project_archive {
bool Publish(const std::filesystem::path &temporary,
             const std::filesystem::path &target, std::string &error) {
  std::error_code ec;
#ifdef _WIN32
  if (!MoveFileExW(temporary.wstring().c_str(), target.wstring().c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    ec = std::error_code(static_cast<int>(GetLastError()), std::system_category());
#else
  std::filesystem::rename(temporary, target, ec);
#endif
  error = ec.message();
  return !ec;
}
}
