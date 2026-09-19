#pragma once

#include <algorithm>
#include <string>

namespace perastage::archive {

// Converts archive separators to the portable forward-slash representation.
inline std::string NormalizeEntrySeparators(std::string path) {
  std::replace(path.begin(), path.end(), '\\', '/');
  return path;
}

// Reports whether a normalized archive entry is unsafe to trust or materialize.
inline bool IsUnsafeNormalizedEntryPath(const std::string &path) {
  if (path.empty() || path.front() == '/' ||
      path.find(':') != std::string::npos)
    return true;

  size_t start = 0;
  while (start <= path.size()) {
    const size_t slash = path.find('/', start);
    const std::string part = path.substr(
        start, slash == std::string::npos ? std::string::npos : slash - start);
    if (part == "..")
      return true;
    if (slash == std::string::npos)
      break;
    start = slash + 1;
  }
  return false;
}

} // namespace perastage::archive
