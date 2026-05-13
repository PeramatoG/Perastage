#pragma once

#include <string>

namespace tools {

struct FixtureSymbolCaptureSanitizeResult {
  bool ok = false;
  std::string sanitizedGdtfPath;
  std::string error;
  int removedSvgEntries = 0;
  int removedSvgOffsetAttributes = 0;
};

// Build a temporary sanitized GDTF archive used only for fixture symbol capture.
FixtureSymbolCaptureSanitizeResult BuildSanitizedFixtureCaptureGdtf(
    const std::string &sourceGdtfPath);

} // namespace tools
