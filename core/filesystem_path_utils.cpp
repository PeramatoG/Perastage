#include "filesystem_path_utils.h"

#include <stdexcept>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace PathUtils {

// Creates a filesystem path from UTF-8 text using the native platform encoding.
std::filesystem::path PathFromUtf8(const std::string &text) {
  if (text.empty())
    return {};

#ifdef _WIN32
  const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                       text.data(),
                                       static_cast<int>(text.size()), nullptr, 0);
  if (size <= 0)
    throw std::runtime_error("Failed to convert UTF-8 path to a Windows path");

  std::wstring wide(static_cast<std::size_t>(size), L'\0');
  const int converted = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                            text.data(),
                                            static_cast<int>(text.size()),
                                            wide.data(), size);
  if (converted != size)
    throw std::runtime_error("Failed to convert UTF-8 path to a Windows path");
  return std::filesystem::path(wide);
#else
  return std::filesystem::path(text);
#endif
}

// Converts a filesystem path to UTF-8 text for storage and cross-platform APIs.
std::string PathToUtf8(const std::filesystem::path &path) {
#ifdef _WIN32
  const std::wstring wide = path.wstring();
  if (wide.empty())
    return {};

  const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                                       wide.data(),
                                       static_cast<int>(wide.size()), nullptr, 0,
                                       nullptr, nullptr);
  if (size <= 0)
    throw std::runtime_error("Failed to convert Windows path to UTF-8");

  std::string text(static_cast<std::size_t>(size), '\0');
  const int converted = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                                            wide.data(),
                                            static_cast<int>(wide.size()),
                                            text.data(), size, nullptr, nullptr);
  if (converted != size)
    throw std::runtime_error("Failed to convert Windows path to UTF-8");
  return text;
#else
  return path.string();
#endif
}

} // namespace PathUtils
