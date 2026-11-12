#include "symbols/fixture_symbol_operation_counters.h"
#include "fixtures/fixture_gdtf_resolution.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <sstream>
#include <unordered_map>
#include <string>
#include <vector>

#include <wx/log.h>

#include "projectutils.h"

namespace fs = std::filesystem;

namespace gui::fixtures {
namespace {

std::string BuildLogPrefix(const Fixture &fixture,
                           const MvrScene &scene,
                           const fs::path &specPath,
                           const std::string &traceContext) {
  std::ostringstream out;
  out << "[FixtureGdtfResolution]";
  if (!traceContext.empty())
    out << " context='" << traceContext << "'";
  out << " uuid='" << fixture.uuid << "'"
      << " scene.basePath='" << scene.basePath << "'"
      << " gdtfSpec='" << specPath.string() << "'";
  return out.str();
}

std::string DecodePathEscapes(const std::string &value) {
  std::string out;
  out.reserve(value.size());
  for (size_t i = 0; i < value.size(); ++i) {
    if (i + 2 < value.size() && value[i] == '%' && value[i + 1] == '2' &&
        value[i + 2] == '0') {
      out.push_back(' ');
      i += 2;
      continue;
    }
    out.push_back(value[i]);
  }
  return out;
}

std::string TrimPathRef(std::string value) {
  auto isTrim = [](unsigned char c) {
    return std::isspace(c) != 0 || c == '\r' || c == '\n' || c == '\t';
  };

  while (!value.empty() && isTrim(static_cast<unsigned char>(value.front())))
    value.erase(value.begin());
  while (!value.empty() && isTrim(static_cast<unsigned char>(value.back())))
    value.pop_back();

  if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
    return value.substr(1, value.size() - 2);
  return value;
}

std::string NormalizePathSeparators(std::string value) {
  std::replace(value.begin(), value.end(), '\\',
               static_cast<char>(fs::path::preferred_separator));
  std::replace(value.begin(), value.end(), '/',
               static_cast<char>(fs::path::preferred_separator));
  return value;
}

// Bound diagnostic memory without caching resolution outcomes or errors.
auto &LoggedSelections() {
  static thread_local std::unordered_map<std::string, std::string> selections;
  return selections;
}

void LogSelection(const std::string &prefix, const std::string &path) {
  auto &selections = LoggedSelections();
  auto previous = selections.find(prefix);
  if (previous != selections.end() && previous->second == path)
    return;
  if (selections.size() >= 1024)
    selections.clear();
  selections[prefix] = path;
  wxLogDebug("%s selected candidate '%s'.", prefix.c_str(), path.c_str());
}

} // namespace

bool ResolveFixtureGdtfDeterministic(const Fixture &fixture,
                                     const MvrScene &scene,
                                     FixtureGdtfResolution &resolution,
                                     std::string &errorMessage,
                                     const std::string &traceContext) {
  symbols::diagnostics::Count(symbols::diagnostics::Operation::Resolution);
  resolution = {};

  const std::string sanitizedSpec =
      NormalizePathSeparators(DecodePathEscapes(TrimPathRef(fixture.gdtfSpec)));
  const fs::path specPath = fs::path(sanitizedSpec);
  const std::string logPrefix =
      BuildLogPrefix(fixture, scene, specPath, traceContext);

  if (specPath.empty()) {
    errorMessage = "Fixture gdtfSpec is empty; deterministic resolution requires a valid path or file name.";
    LoggedSelections().erase(logPrefix);
    wxLogDebug("%s failed: %s", logPrefix.c_str(), errorMessage.c_str());
    return false;
  }

  std::vector<std::string> discarded;
  const auto discard = [&](const std::string &label,
                           const fs::path &path, const std::string &reason) {
    discarded.push_back(label + " candidate '" + path.string() + "': " + reason);
  };
  std::error_code ec;
  if (specPath.is_absolute()) {
    ec.clear();
    symbols::diagnostics::Count(symbols::diagnostics::Operation::FilesystemResolution);
    if (fs::exists(specPath, ec) && !ec) {
      resolution.selectedPath = specPath.string();
      LogSelection(logPrefix, resolution.selectedPath);
      return true;
    }
    discard("absolute", specPath,
                     ec ? ec.message() : "path does not exist");
  }

  if (!scene.basePath.empty() && !specPath.is_absolute()) {
    const fs::path sceneCandidate = fs::path(scene.basePath) / specPath;
    ec.clear();
    symbols::diagnostics::Count(symbols::diagnostics::Operation::FilesystemResolution);
    if (fs::exists(sceneCandidate, ec) && !ec) {
      resolution.scenePath = sceneCandidate.string();
      resolution.selectedPath = resolution.scenePath;
      LogSelection(logPrefix, resolution.selectedPath);
      return true;
    }
    discard("scene", sceneCandidate,
                     ec ? ec.message() : "path does not exist");
  } else if (scene.basePath.empty()) {
    discard("scene", {}, "scene.basePath is empty");
  } else {
    discard("scene", specPath,
                     "gdtfSpec is absolute, scene-relative resolution skipped");
  }

  const std::string fileName = specPath.filename().string();
  if (fileName.empty()) {
    errorMessage =
        "Fixture gdtfSpec has no file name; cannot resolve fixtures library candidate.";
    LoggedSelections().erase(logPrefix);
    wxLogDebug("%s failed: %s", logPrefix.c_str(), errorMessage.c_str());
    return false;
  }

  const fs::path fixturesLibrary =
      fs::path(ProjectUtils::GetDefaultLibraryPath("fixtures"));
  const fs::path libraryCandidate = fixturesLibrary / fileName;
  ec.clear();
  symbols::diagnostics::Count(symbols::diagnostics::Operation::FilesystemResolution);
  if (fs::exists(libraryCandidate, ec) && !ec) {
    resolution.libraryPath = libraryCandidate.string();
    resolution.selectedPath = resolution.libraryPath;
    LogSelection(logPrefix, resolution.selectedPath);
    return true;
  }

  discard("library", libraryCandidate,
                   ec ? ec.message() : "path does not exist");

  std::ostringstream out;
  out << "Could not resolve fixture GDTF path deterministically for fixture uuid '"
      << fixture.uuid << "'."
      << " scene.basePath='" << scene.basePath << "'"
      << " gdtfSpec='" << fixture.gdtfSpec << "'.";
  for (const auto &reason : discarded)
    out << " " << reason << ";";
  errorMessage = out.str();
  LoggedSelections().erase(logPrefix);
  wxLogDebug("%s failed: %s", logPrefix.c_str(), errorMessage.c_str());
  return false;
}

} // namespace gui::fixtures
