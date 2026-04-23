#include "fixtures/fixture_gdtf_resolution.h"

#include <filesystem>
#include <sstream>

#include <wx/log.h>

#include "projectutils.h"

namespace fs = std::filesystem;

namespace gui::fixtures {
namespace {

std::string BuildLogPrefix(const Fixture &fixture,
                           const MvrScene &scene,
                           const fs::path &specPath) {
  std::ostringstream out;
  out << "[FixtureGdtfResolution] uuid='" << fixture.uuid << "'"
      << " scene.basePath='" << scene.basePath << "'"
      << " gdtfSpec='" << specPath.string() << "'";
  return out.str();
}

void LogDiscardReason(const std::string &prefix,
                      const std::string &candidateLabel,
                      const fs::path &candidatePath,
                      const std::string &reason) {
  wxLogDebug("%s discarded %s candidate '%s': %s", prefix.c_str(),
             candidateLabel.c_str(), candidatePath.string().c_str(), reason.c_str());
}

} // namespace

bool ResolveFixtureGdtfDeterministic(const Fixture &fixture,
                                     const MvrScene &scene,
                                     FixtureGdtfResolution &resolution,
                                     std::string &errorMessage) {
  resolution = {};

  const fs::path specPath = fs::path(fixture.gdtfSpec);
  const std::string logPrefix = BuildLogPrefix(fixture, scene, specPath);

  if (specPath.empty()) {
    errorMessage = "Fixture gdtfSpec is empty; deterministic resolution requires a valid path or file name.";
    wxLogDebug("%s failed: %s", logPrefix.c_str(), errorMessage.c_str());
    return false;
  }

  std::error_code ec;
  if (specPath.is_absolute()) {
    ec.clear();
    if (fs::exists(specPath, ec) && !ec) {
      resolution.selectedPath = specPath.string();
      wxLogDebug("%s selected absolute candidate '%s'.", logPrefix.c_str(),
                 resolution.selectedPath.c_str());
      return true;
    }
    LogDiscardReason(logPrefix, "absolute", specPath,
                     ec ? ec.message() : "path does not exist");
  }

  if (!scene.basePath.empty() && !specPath.is_absolute()) {
    const fs::path sceneCandidate = fs::path(scene.basePath) / specPath;
    ec.clear();
    if (fs::exists(sceneCandidate, ec) && !ec) {
      resolution.scenePath = sceneCandidate.string();
      resolution.selectedPath = resolution.scenePath;
      wxLogDebug("%s selected scene candidate '%s'.", logPrefix.c_str(),
                 resolution.selectedPath.c_str());
      return true;
    }
    LogDiscardReason(logPrefix, "scene", sceneCandidate,
                     ec ? ec.message() : "path does not exist");
  } else if (scene.basePath.empty()) {
    LogDiscardReason(logPrefix, "scene", {}, "scene.basePath is empty");
  } else {
    LogDiscardReason(logPrefix, "scene", specPath,
                     "gdtfSpec is absolute, scene-relative resolution skipped");
  }

  const std::string fileName = specPath.filename().string();
  if (fileName.empty()) {
    errorMessage =
        "Fixture gdtfSpec has no file name; cannot resolve fixtures library candidate.";
    wxLogDebug("%s failed: %s", logPrefix.c_str(), errorMessage.c_str());
    return false;
  }

  const fs::path fixturesLibrary =
      fs::path(ProjectUtils::GetDefaultLibraryPath("fixtures"));
  const fs::path libraryCandidate = fixturesLibrary / fileName;
  ec.clear();
  if (fs::exists(libraryCandidate, ec) && !ec) {
    resolution.libraryPath = libraryCandidate.string();
    resolution.selectedPath = resolution.libraryPath;
    wxLogDebug("%s selected library candidate '%s'.", logPrefix.c_str(),
               resolution.selectedPath.c_str());
    return true;
  }

  LogDiscardReason(logPrefix, "library", libraryCandidate,
                   ec ? ec.message() : "path does not exist");

  std::ostringstream out;
  out << "Could not resolve fixture GDTF path deterministically for fixture uuid '"
      << fixture.uuid << "'."
      << " scene.basePath='" << scene.basePath << "'"
      << " gdtfSpec='" << fixture.gdtfSpec << "'.";
  errorMessage = out.str();
  wxLogDebug("%s failed: %s", logPrefix.c_str(), errorMessage.c_str());
  return false;
}

} // namespace gui::fixtures
