#pragma once

#include <string>

namespace symbol_cache {

// Identifies every output-relevant input to fixture-symbol generation.
struct FixtureSymbolGenerationIdentity {
  std::string key;
  std::string portableGdtfIdentity;
  std::string gdtfMode;
  int symbolFormatVersion = 0;
  std::string semanticFingerprint;
  std::string displayLabel;

  // Compares generation inputs while deliberately excluding presentation metadata.
  bool operator==(const FixtureSymbolGenerationIdentity &other) const;
};

// Builds a canonical identity from a portable GDTF reference and audited inputs.
bool BuildFixtureSymbolGenerationIdentity(
    const std::string &portableGdtfIdentity, const std::string &gdtfMode,
    int symbolFormatVersion, const std::string &semanticFingerprint,
    const std::string &displayLabel, FixtureSymbolGenerationIdentity &identity,
    std::string &errorMessage);

// Normalizes a project-owned archive reference without accepting host paths.
bool NormalizePortableGdtfIdentity(const std::string &input,
                                   std::string &normalized,
                                   std::string &errorMessage);

// Converts an absolute project-owned path to a portable relative identity.
std::string WindowsPathToPortable(const std::string &path,
                                  const std::string &projectRoot);

} // namespace symbol_cache
