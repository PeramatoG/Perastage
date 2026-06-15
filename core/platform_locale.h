#pragma once

#include <string>

namespace platform {

struct LocaleSetupResult {
  bool changed = false;
  std::string activeLocale;
  std::string note;
};

LocaleSetupResult EnsureProcessTextLocale();

} // namespace platform
