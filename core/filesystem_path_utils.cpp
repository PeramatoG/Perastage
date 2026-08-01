#include "filesystem_path_utils.h"

#include <algorithm>
#include <cwctype>
#include <stdexcept>
#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace PathUtils {


// Resolves a path for identity-key generation without throwing on filesystem errors.
static std::filesystem::path ResolveIdentityPath(const std::filesystem::path &path) {
  if (path.empty())
    return {};

  std::error_code ec;
  std::filesystem::path resolved = std::filesystem::weakly_canonical(path, ec);
  if (ec) {
    ec.clear();
    resolved = std::filesystem::absolute(path, ec);
  }
  if (ec)
    resolved = path;
  return resolved.lexically_normal();
}

// Normalizes platform path spelling for cache identity while preserving UTF-8 validity.
static std::string NormalizeIdentityText(std::filesystem::path path) {
  path.make_preferred();
  std::string text = PathToUtf8(path.lexically_normal());
  std::replace(text.begin(), text.end(), '\\', '/');
#ifdef _WIN32
  std::wstring wide = path.lexically_normal().wstring();
  std::replace(wide.begin(), wide.end(), L'\\', L'/');
  std::transform(wide.begin(), wide.end(), wide.begin(),
                 [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
  return PathToUtf8(std::filesystem::path(wide));
#else
  return text;
#endif
}

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


// Builds an internal filesystem identity key for cache and deduplication lookups.
std::string BuildFilesystemIdentityKey(const std::filesystem::path &path) {
  return NormalizeIdentityText(ResolveIdentityPath(path));
}

// Builds an internal filesystem identity key from UTF-8 path text.
std::string BuildFilesystemIdentityKey(const std::string &utf8Path) {
  return BuildFilesystemIdentityKey(PathFromUtf8(utf8Path));
}

// Builds an internal identity key after resolving a relative path against a base path.
std::string BuildFilesystemIdentityKey(const std::filesystem::path &path,
                                       const std::filesystem::path &basePath) {
  if (path.empty())
    return {};
  if (path.is_relative() && !basePath.empty())
    return BuildFilesystemIdentityKey(basePath / path);
  return BuildFilesystemIdentityKey(path);
}

// Determines whether two paths identify the same physical or canonical location.
bool AreFilesystemPathsEquivalent(const std::filesystem::path &left,
                                  const std::filesystem::path &right,
                                  std::error_code &error) {
  error.clear();
  if (left.empty() || right.empty())
    return false;
  const bool equivalent = std::filesystem::equivalent(left, right, error);
  if (!error)
    return equivalent;
  error.clear();
  const std::filesystem::path canonicalLeft =
      std::filesystem::weakly_canonical(left, error);
  if (error)
    return false;
  const std::filesystem::path canonicalRight =
      std::filesystem::weakly_canonical(right, error);
  return !error && canonicalLeft == canonicalRight;
}

} // namespace PathUtils
