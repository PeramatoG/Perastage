#include "fixture_symbol_generation_identity.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string_view>
#include <vector>

namespace symbol_cache {
namespace {

// Appends one unambiguous length-prefixed field to the canonical representation.
void AppendField(std::string &output, std::string_view field) {
  output += std::to_string(field.size());
  output.push_back(':');
  output.append(field);
}

// Parses a Windows absolute path without relying on host filesystem semantics.
bool ParseWindowsAbsolutePath(std::string value, char &drive,
                              std::vector<std::string> &components) {
  components.clear();
  if (value.size() < 3 || !std::isalpha(static_cast<unsigned char>(value[0])) ||
      value[1] != ':' || (value[2] != '/' && value[2] != '\\'))
    return false;
  drive = static_cast<char>(std::tolower(static_cast<unsigned char>(value[0])));
  std::replace(value.begin(), value.end(), '\\', '/');
  std::string component;
  for (std::size_t i = 3; i <= value.size(); ++i) {
    if (i == value.size() || value[i] == '/') {
      if (component.empty() || component == ".") {
        component.clear();
        continue;
      }
      if (component == "..") {
        if (components.empty())
          return false;
        components.pop_back();
      } else {
        components.push_back(component);
      }
      component.clear();
    } else {
      component.push_back(value[i]);
    }
  }
  return true;
}

// Compares Windows path components using deterministic ASCII case folding.
bool EqualWindowsComponent(std::string_view left, std::string_view right) {
  if (left.size() != right.size())
    return false;
  for (std::size_t i = 0; i < left.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(left[i])) !=
        std::tolower(static_cast<unsigned char>(right[i])))
      return false;
  }
  return true;
}

} // namespace

// Converts an absolute project-owned path to a portable relative identity.
std::string WindowsPathToPortable(const std::string &path,
                                  const std::string &projectRoot) {
  char pathDrive = 0;
  char rootDrive = 0;
  std::vector<std::string> pathComponents;
  std::vector<std::string> rootComponents;
  const bool pathIsWindows =
      ParseWindowsAbsolutePath(path, pathDrive, pathComponents);
  const bool rootIsWindows =
      ParseWindowsAbsolutePath(projectRoot, rootDrive, rootComponents);
  if (pathIsWindows || rootIsWindows) {
    if (!pathIsWindows || !rootIsWindows || pathDrive != rootDrive ||
        rootComponents.size() >= pathComponents.size())
      return {};
    for (std::size_t i = 0; i < rootComponents.size(); ++i) {
      if (!EqualWindowsComponent(pathComponents[i], rootComponents[i]))
        return {};
    }
    std::string relative;
    for (std::size_t i = rootComponents.size(); i < pathComponents.size(); ++i) {
      if (!relative.empty())
        relative.push_back('/');
      relative += pathComponents[i];
    }
    return relative;
  }

  const std::filesystem::path normalizedPath =
      std::filesystem::path(path).lexically_normal();
  const std::filesystem::path normalizedRoot =
      std::filesystem::path(projectRoot).lexically_normal();
  if (!normalizedPath.is_absolute() || !normalizedRoot.is_absolute())
    return {};
  const std::filesystem::path relative =
      normalizedPath.lexically_relative(normalizedRoot);
  if (relative.empty() || *relative.begin() == "..")
    return {};
  return relative.generic_string();
}

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
