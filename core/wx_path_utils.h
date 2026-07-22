#pragma once

#include <filesystem>

#include <wx/string.h>

namespace WxPathUtils {

// Converts a filesystem path to a wxString without using the active narrow code page.
inline wxString WxStringFromFilesystemPath(const std::filesystem::path &path) {
  if (path.empty())
    return wxString();
#if defined(_WIN32)
  return wxString(path.native());
#else
  const std::u8string utf8 = path.u8string();
  return wxString::FromUTF8(reinterpret_cast<const char *>(utf8.c_str()));
#endif
}

// Converts a wxString path to std::filesystem::path through the native platform encoding.
inline std::filesystem::path FilesystemPathFromWxString(const wxString &value) {
  if (value.empty())
    return std::filesystem::path();
#if defined(_WIN32)
  return std::filesystem::path(value.wc_str().data());
#else
  const wxScopedCharBuffer utf8 = value.utf8_str();
  return std::filesystem::path(reinterpret_cast<const char8_t *>(utf8.data()));
#endif
}

} // namespace WxPathUtils
