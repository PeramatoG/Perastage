#include "fixture_symbol_generation_identity.h"

#include <cctype>
#include <string_view>

namespace symbol_cache {
namespace {

// Appends one unambiguous length-prefixed field to the canonical representation.
void AppendField(std::string &output, std::string_view field) {
  output += std::to_string(field.size());
  output.push_back(':');
  output.append(field);
}

} // namespace

// Compares generation inputs while deliberately excluding presentation metadata.
bool FixtureSymbolGenerationIdentity::operator==(
    const FixtureSymbolGenerationIdentity &other) const {
  return key == other.key && portableGdtfIdentity == other.portableGdtfIdentity &&
         gdtfMode == other.gdtfMode &&
         symbolFormatVersion == other.symbolFormatVersion &&
         semanticFingerprint == other.semanticFingerprint;
}

// Normalizes a project-owned archive reference without accepting host paths.
bool NormalizePortableGdtfIdentity(const std::string &input,
                                   std::string &normalized,
                                   std::string &errorMessage) {
  normalized.clear();
  errorMessage.clear();
  if (input.empty() || input.front() == '/' || input.find('\\') != std::string::npos ||
      (input.size() >= 2 && std::isalpha(static_cast<unsigned char>(input[0])) &&
       input[1] == ':')) {
    errorMessage = "GDTF identity must be a portable relative archive reference.";
    return false;
  }
  std::string component;
  for (unsigned char ch : input) {
    if (ch < 32 || ch == 127) {
      errorMessage = "GDTF identity contains a control character.";
      return false;
    }
    if (ch == '/') {
      if (component.empty() || component == "." || component == "..") {
        errorMessage = "GDTF identity contains an unsafe path component.";
        return false;
      }
      if (!normalized.empty())
        normalized.push_back('/');
      normalized += component;
      component.clear();
    } else {
      component.push_back(static_cast<char>(ch));
    }
  }
  if (component.empty() || component == "." || component == "..") {
    errorMessage = "GDTF identity contains an unsafe path component.";
    return false;
  }
  if (!normalized.empty())
    normalized.push_back('/');
  normalized += component;
  return true;
}

// Builds a canonical identity from a portable GDTF reference and audited inputs.
bool BuildFixtureSymbolGenerationIdentity(
    const std::string &portableGdtfIdentity, const std::string &gdtfMode,
    int symbolFormatVersion, const std::string &semanticFingerprint,
    const std::string &displayLabel, FixtureSymbolGenerationIdentity &identity,
    std::string &errorMessage) {
  identity = {};
  if (symbolFormatVersion <= 0 || semanticFingerprint.empty()) {
    errorMessage = "Symbol format version and semantic fingerprint are required.";
    return false;
  }
  if (!NormalizePortableGdtfIdentity(portableGdtfIdentity,
                                     identity.portableGdtfIdentity,
                                     errorMessage))
    return false;
  identity.gdtfMode = gdtfMode;
  identity.symbolFormatVersion = symbolFormatVersion;
  identity.semanticFingerprint = semanticFingerprint;
  identity.displayLabel = displayLabel;
  identity.key = "perastage-fixture-symbol-generation:v1|";
  AppendField(identity.key, identity.portableGdtfIdentity);
  AppendField(identity.key, identity.gdtfMode);
  AppendField(identity.key, std::to_string(identity.symbolFormatVersion));
  AppendField(identity.key, identity.semanticFingerprint);
  return true;
}

} // namespace symbol_cache
