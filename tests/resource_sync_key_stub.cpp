#include <string>

// Builds the mode-aware key used by lightweight bounds-cache test targets.
std::string BuildGdtfResourceKey(const std::string &resolvedPath,
                                 const std::string &modeName) {
  return resolvedPath + "\nmode=" + modeName;
}
