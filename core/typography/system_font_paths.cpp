#include "system_font_paths.h"
#include <cstdlib>

namespace typography {
std::vector<std::filesystem::path> SystemFontRoots() {
  std::vector<std::filesystem::path> roots;
  const auto add = [&](const char *variable, const char *suffix) {
#ifdef _WIN32
    if (const auto *value = _wgetenv(std::filesystem::path(variable).c_str()); value && *value)
#else
    if (const auto *value = std::getenv(variable); value && *value)
#endif
      roots.push_back(std::filesystem::path(value) / suffix);
  };
#ifdef _WIN32
  add("WINDIR", "Fonts");
  add("LOCALAPPDATA", "Microsoft/Windows/Fonts");
#elif defined(__APPLE__)
  roots = {"/System/Library/Fonts", "/Library/Fonts"};
  add("HOME", "Library/Fonts");
#else
  roots = {"/usr/share/fonts", "/usr/local/share/fonts"};
  add("XDG_DATA_HOME", "fonts");
  add("HOME", ".local/share/fonts");
  add("HOME", ".fonts");
#endif
  return roots;
}
}
