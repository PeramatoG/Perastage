#pragma once

#include <string>

namespace diagnostics {

// Installs local crash and unhandled-exception diagnostics.
class CrashHandler {
public:
  static void Initialize();
  static void WriteExceptionReport(const std::string &reason,
                                   const std::string &details = {});
};

} // namespace diagnostics
