#include "configservices.h"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

int main() {
  namespace fs = std::filesystem;

  const fs::path tempDir = fs::temp_directory_path() / "perastage_project_session_io_test";
  std::error_code ec;
  fs::remove_all(tempDir, ec);
  fs::create_directories(tempDir, ec);

  const fs::path projectPath = tempDir / "session_test.pera";
  ProjectSession saveSession;

  bool saveConfigCalled = false;
  bool saveSceneCalled = false;
  const bool saveOk = saveSession.SaveProject(
      projectPath.string(),
      [&](const std::string &path) {
        saveConfigCalled = true;
        std::ofstream out(path, std::ios::binary);
        out << "{\"mode\":\"test\"}";
        return out.good();
      },
      [&](const std::string &path) {
        saveSceneCalled = true;
        std::ofstream out(path, std::ios::binary);
        out << "PKSCENE";
        return out.good();
      });

  assert(saveOk);
  assert(saveConfigCalled);
  assert(saveSceneCalled);

  ProjectSession loadSession;
  bool loadConfigCalled = false;
  bool loadSceneCalled = false;
  const bool loadOk = loadSession.LoadProject(
      projectPath.string(),
      [&](const std::string &path) {
        loadConfigCalled = true;
        std::ifstream in(path, std::ios::binary);
        std::string body;
        std::getline(in, body, '\0');
        return body.find("mode") != std::string::npos;
      },
      [&](const std::string &path) {
        loadSceneCalled = true;
        std::ifstream in(path, std::ios::binary);
        std::string body;
        std::getline(in, body, '\0');
        return body == "PKSCENE";
      });

  assert(loadOk);
  assert(loadConfigCalled);
  assert(loadSceneCalled);

  const fs::path zeroSceneProjectPath = tempDir / "zero_scene.pera";
  ProjectSession zeroSceneSession;
  const bool zeroSceneSaveOk = zeroSceneSession.SaveProject(
      zeroSceneProjectPath.string(),
      [](std::vector<std::uint8_t> &out) {
        const std::string config = "{\"mode\":\"test\"}";
        out.assign(config.begin(), config.end());
        return true;
      },
      [](std::vector<std::uint8_t> &out) {
        out.clear();
        return true;
      });
  assert(!zeroSceneSaveOk);
  assert(!fs::exists(zeroSceneProjectPath));

  const fs::path requiredResourceFailurePath =
      tempDir / "required_resource_failure.pera";
  const bool requiredResourceFailure = zeroSceneSession.SaveProject(
      requiredResourceFailurePath.string(),
      [](std::vector<std::uint8_t> &out) {
        out = {'{', '}'};
        return true;
      },
      [](std::vector<std::uint8_t> &out) {
        out = {'P', 'K'};
        return true;
      },
      ProjectSession::CollectArchiveResourcesFn([] {
        return std::vector<ProjectSession::ArchiveResource>{
            {"../unsafe-manifest.json", {'{', '}'}, true}};
      }));
  assert(!requiredResourceFailure);
  assert(!fs::exists(requiredResourceFailurePath));

  const fs::path jsonPath = tempDir / "config_only.json";
  {
    std::ofstream out(jsonPath, std::ios::binary);
    out << "{\"hello\":\"world\"}";
  }

  bool jsonConfigCalled = false;
  bool jsonSceneCalled = false;
  const bool jsonOk = loadSession.LoadProject(
      jsonPath.string(),
      [&](const std::string &path) {
        jsonConfigCalled = true;
        return path == jsonPath.string();
      },
      [&](const std::string &) {
        jsonSceneCalled = true;
        return false;
      });

  assert(jsonOk);
  assert(jsonConfigCalled);
  assert(!jsonSceneCalled);

  fs::remove_all(tempDir, ec);
  return 0;
}
